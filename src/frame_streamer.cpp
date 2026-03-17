#include "catcheye/core/frame_streamer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <opencv2/imgcodecs.hpp>

#include "catcheye/utils/logger.hpp"

namespace catcheye {

FrameStreamer::FrameStreamer(FrameStreamerConfig config)
    : config_(std::move(config)) {}

FrameStreamer::~FrameStreamer() {
    stop();
}

bool FrameStreamer::start() {
    if (running_) {
        return true;
    }

    ::unlink(config_.socket_path.c_str());

    server_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        if (const auto log = logger()) {
            log->error("frame_streamer: failed to create socket: {}", std::strerror(errno));
        }
        return false;
    }

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, config_.socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        if (const auto log = logger()) {
            log->error("frame_streamer: failed to bind '{}': {}", config_.socket_path, std::strerror(errno));
        }
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (::listen(server_fd_, config_.max_clients) < 0) {
        if (const auto log = logger()) {
            log->error("frame_streamer: listen failed: {}", std::strerror(errno));
        }
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Set non-blocking for accept loop
    int flags = ::fcntl(server_fd_, F_GETFL, 0);
    ::fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

    running_ = true;
    accept_thread_ = std::thread(&FrameStreamer::accept_loop, this);

    if (const auto log = logger()) {
        log->info("frame_streamer: listening on '{}'", config_.socket_path);
    }
    return true;
}

void FrameStreamer::stop() {
    running_ = false;

    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (int fd : client_fds_) {
            ::close(fd);
        }
        client_fds_.clear();
    }

    ::unlink(config_.socket_path.c_str());

    if (const auto log = logger()) {
        log->info("frame_streamer: stopped");
    }
}

bool FrameStreamer::is_running() const {
    return running_;
}

void FrameStreamer::accept_loop() {
    while (running_) {
        struct pollfd pfd {};
        pfd.fd = server_fd_;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 200);
        if (ret <= 0) {
            continue;
        }

        int client_fd = ::accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            continue;
        }

        // Set send timeout to avoid blocking on slow clients
        struct timeval tv {};
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50ms
        ::setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        // Enable TCP_NODELAY equivalent for Unix sockets (reduce buffering)
        int sndbuf = 512 * 1024;
        ::setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            if (static_cast<int>(client_fds_.size()) >= config_.max_clients) {
                if (const auto log = logger()) {
                    log->warn("frame_streamer: max clients reached, rejecting connection");
                }
                ::close(client_fd);
            } else {
                client_fds_.push_back(client_fd);
                if (const auto log = logger()) {
                    log->info("frame_streamer: client connected (total: {})", client_fds_.size());
                }
            }
        }
    }
}

void FrameStreamer::send_frame(const cv::Mat& frame) {
    if (!running_ || frame.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);
    if (client_fds_.empty()) {
        return;
    }

    // Encode to JPEG
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, config_.jpeg_quality};
    jpeg_buffer_.clear();
    if (!cv::imencode(".jpg", frame, jpeg_buffer_, params)) {
        return;
    }

    // Build header: 4-byte little-endian frame size
    const auto frame_size = static_cast<std::uint32_t>(jpeg_buffer_.size());
    std::uint8_t header[4];
    header[0] = static_cast<std::uint8_t>(frame_size & 0xFF);
    header[1] = static_cast<std::uint8_t>((frame_size >> 8) & 0xFF);
    header[2] = static_cast<std::uint8_t>((frame_size >> 16) & 0xFF);
    header[3] = static_cast<std::uint8_t>((frame_size >> 24) & 0xFF);

    // Send to all clients, mark disconnected ones
    std::vector<int> failed_fds;
    for (int fd : client_fds_) {
        // Send header
        ssize_t sent = ::send(fd, header, 4, MSG_NOSIGNAL);
        if (sent != 4) {
            failed_fds.push_back(fd);
            continue;
        }

        // Send JPEG data
        std::size_t total_sent = 0;
        while (total_sent < jpeg_buffer_.size()) {
            sent = ::send(
                fd,
                jpeg_buffer_.data() + total_sent,
                jpeg_buffer_.size() - total_sent,
                MSG_NOSIGNAL);
            if (sent <= 0) {
                failed_fds.push_back(fd);
                break;
            }
            total_sent += static_cast<std::size_t>(sent);
        }
    }

    // Remove failed clients
    for (int fd : failed_fds) {
        ::close(fd);
        client_fds_.erase(
            std::remove(client_fds_.begin(), client_fds_.end(), fd),
            client_fds_.end());
    }

    if (!failed_fds.empty()) {
        if (const auto log = logger()) {
            log->info(
                "frame_streamer: removed {} disconnected client(s), remaining: {}",
                failed_fds.size(),
                client_fds_.size());
        }
    }
}

} // namespace catcheye
