#include "catcheye/core/frame_streamer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <span>
#include <string>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <opencv2/imgcodecs.hpp>

#include "catcheye/utils/logger.hpp"

namespace catcheye {

namespace {

constexpr std::string_view MJPEG_HTTP_HEADER =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace;boundary=frame\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n";

constexpr std::string_view MJPEG_FRAME_HEADER =
    "--frame\r\n"
    "Content-Type: image/jpeg\r\n";

constexpr int POLL_TIMEOUT_MS = 200;
constexpr int SEND_TIMEOUT_US = 50000;
constexpr int SOCKET_ENABLE = 1;

bool send_all(int sock_fd, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::byte*>(data);
    std::span<const std::byte> remaining{bytes, size};
    while (!remaining.empty()) {
        const ssize_t WRITTEN = ::send(sock_fd, remaining.data(), remaining.size(), MSG_NOSIGNAL);
        if (WRITTEN <= 0) {
            return false;
        }
        remaining = remaining.subspan(static_cast<std::size_t>(WRITTEN));
    }
    return true;
}

} // namespace

FrameStreamer::FrameStreamer(FrameStreamerConfig config)
    : config_(config) {}

FrameStreamer::~FrameStreamer() {
    stop();
}

bool FrameStreamer::start() {
    if (running_) {
        return true;
    }

    // SOCK_NONBLOCK avoids a separate fcntl call to set non-blocking mode
    server_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd_ < 0) {
        if (auto logger_ptr = logger()) {
            logger_ptr->error("frame_streamer: failed to create socket: {}", std::strerror(errno));
        }
        return false;
    }

    ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &SOCKET_ENABLE, sizeof(SOCKET_ENABLE));

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<std::uint16_t>(config_.port));

    // Cast through void* to avoid reinterpret_cast
    void* addr_ptr = &addr;
    if (::bind(server_fd_, static_cast<struct sockaddr*>(addr_ptr), sizeof(addr)) < 0) {
        if (auto logger_ptr = logger()) {
            logger_ptr->error("frame_streamer: failed to bind port {}: {}", config_.port, std::strerror(errno));
        }
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (::listen(server_fd_, config_.max_clients) < 0) {
        if (auto logger_ptr = logger()) {
            logger_ptr->error("frame_streamer: listen failed: {}", std::strerror(errno));
        }
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    running_ = true;
    accept_thread_ = std::thread(&FrameStreamer::accept_loop, this);

    if (auto logger_ptr = logger()) {
        logger_ptr->info("frame_streamer: MJPEG stream listening on port {}", config_.port);
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
        for (int sock_fd : client_fds_) {
            ::close(sock_fd);
        }
        client_fds_.clear();
    }

    if (auto logger_ptr = logger()) {
        logger_ptr->info("frame_streamer: stopped");
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

        if (::poll(&pfd, 1, POLL_TIMEOUT_MS) <= 0) {
            continue;
        }

        int client_sock = ::accept(server_fd_, nullptr, nullptr);
        if (client_sock < 0) {
            continue;
        }

        // Reduce latency: disable Nagle's algorithm
        ::setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &SOCKET_ENABLE, sizeof(SOCKET_ENABLE));

        // Drop frames rather than block on a slow client
        struct timeval send_timeout {};
        send_timeout.tv_usec = SEND_TIMEOUT_US;
        ::setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            if (static_cast<int>(client_fds_.size()) >= config_.max_clients) {
                if (auto logger_ptr = logger()) {
                    logger_ptr->warn("frame_streamer: max clients reached, rejecting connection");
                }
                ::close(client_sock);
                continue;
            }
        }

        // Send MJPEG HTTP response header before handing off to send_frame
        if (!send_all(client_sock, MJPEG_HTTP_HEADER.data(), MJPEG_HTTP_HEADER.size())) {
            ::close(client_sock);
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_fds_.push_back(client_sock);
            if (auto logger_ptr = logger()) {
                logger_ptr->info("frame_streamer: client connected (total: {})", client_fds_.size());
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

    std::vector<int> encode_params = {cv::IMWRITE_JPEG_QUALITY, config_.jpeg_quality};
    jpeg_buffer_.clear();
    if (!cv::imencode(".jpg", frame, jpeg_buffer_, encode_params)) {
        return;
    }

    // Per-frame MJPEG header: boundary + Content-Length
    std::string frame_header =
        std::string(MJPEG_FRAME_HEADER)
        + "Content-Length: " + std::to_string(jpeg_buffer_.size()) + "\r\n"
        + "\r\n";

    std::vector<int> failed_fds;
    for (int sock_fd : client_fds_) {
        if (!send_all(sock_fd, frame_header.data(), frame_header.size())
            || !send_all(sock_fd, jpeg_buffer_.data(), jpeg_buffer_.size())
            || !send_all(sock_fd, "\r\n", 2)) {
            failed_fds.push_back(sock_fd);
        }
    }

    for (int sock_fd : failed_fds) {
        ::close(sock_fd);
        client_fds_.erase(std::remove(client_fds_.begin(), client_fds_.end(), sock_fd), client_fds_.end());
    }

    if (!failed_fds.empty()) {
        if (auto logger_ptr = logger()) {
            logger_ptr->info("frame_streamer: removed {} disconnected client(s), remaining: {}",
                failed_fds.size(), client_fds_.size());
        }
    }
}

} // namespace catcheye
