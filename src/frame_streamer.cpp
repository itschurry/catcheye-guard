#include "catcheye/core/frame_streamer.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <vector>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <opencv2/imgcodecs.hpp>

#include "catcheye/guard/roi/roi_repository.hpp"
#include "catcheye/guard/roi/roi_validation.hpp"
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

constexpr std::string_view CONTENT_LENGTH_PREFIX = "content-length: ";  // lowercase for case-insensitive match
constexpr std::string_view HEADER_SEPARATOR = "\r\n\r\n";

constexpr int POLL_TIMEOUT_MS = 200;
constexpr int SEND_TIMEOUT_US = 50000;   // 50 ms — drop slow MJPEG clients
constexpr int RECV_TIMEOUT_SEC = 2;      // 2 s — abort stalled HTTP reads
constexpr int SOCKET_ENABLE = 1;
constexpr std::size_t MAX_HEADER_BYTES = 8192;
constexpr std::size_t MAX_BODY_BYTES = 65536;   // 64 KB
constexpr std::size_t REQUEST_RESERVE_BYTES = 4096;
constexpr std::size_t RECV_BUFFER_BYTES = 4096;

enum class HTTP_STATUS {
    OK                   = 200,
    BAD_REQUEST          = 400,
    NOT_FOUND            = 404,
    INTERNAL_SERVER_ERROR = 500,
    SERVICE_UNAVAILABLE  = 503,
};

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

// ---------------------------------------------------------------------------
// HTTP request parsing
// ---------------------------------------------------------------------------

struct ParsedRequest {
    std::string method;
    std::string path;
    std::string body;
};

std::optional<ParsedRequest> read_http_request(int sock_fd) {
    std::string raw;
    raw.reserve(REQUEST_RESERVE_BYTES);
    std::array<char, RECV_BUFFER_BYTES> buf {};

    // Read until full header block (\r\n\r\n) or size limit
    while (raw.size() < MAX_HEADER_BYTES) {
        ssize_t received = ::recv(sock_fd, buf.data(), buf.size(), 0);
        if (received <= 0) {
            return std::nullopt;
        }
        raw.append(buf.data(), static_cast<std::size_t>(received));
        if (raw.find(HEADER_SEPARATOR) != std::string::npos) {
            break;
        }
    }

    const std::size_t HEADER_END = raw.find(HEADER_SEPARATOR);
    if (HEADER_END == std::string::npos) {
        return std::nullopt;
    }

    // Parse request line: "METHOD /path HTTP/1.1"
    const std::size_t LINE_END = raw.find("\r\n");
    if (LINE_END == std::string::npos || LINE_END == 0) {
        return std::nullopt;
    }
    const std::string REQUEST_LINE = raw.substr(0, LINE_END);

    const std::size_t SP1 = REQUEST_LINE.find(' ');
    if (SP1 == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t SP2 = REQUEST_LINE.find(' ', SP1 + 1);
    if (SP2 == std::string::npos) {
        return std::nullopt;
    }

    ParsedRequest req;
    req.method = REQUEST_LINE.substr(0, SP1);
    req.path   = REQUEST_LINE.substr(SP1 + 1, SP2 - SP1 - 1);

    // Parse Content-Length for body reads.
    // HTTP headers are case-insensitive (RFC 7230 §3.2); Dart/Flutter sends lowercase.
    // Normalise to lowercase before searching to handle both variants.
    const std::string HEADERS_SECTION = raw.substr(0, HEADER_END);
    std::string headers_lower = HEADERS_SECTION;
    std::transform(headers_lower.begin(), headers_lower.end(), headers_lower.begin(),
                   [](unsigned char chr) { return std::tolower(chr); });

    std::size_t content_length = 0;
    const std::size_t CL_POS = headers_lower.find(CONTENT_LENGTH_PREFIX);
    if (CL_POS != std::string::npos) {
        const std::size_t CL_VALUE_START = CL_POS + CONTENT_LENGTH_PREFIX.size();
        const std::size_t CL_END = HEADERS_SECTION.find("\r\n", CL_VALUE_START);
        const std::string CL_STR = HEADERS_SECTION.substr(
            CL_VALUE_START,
            CL_END == std::string::npos ? std::string::npos : CL_END - CL_VALUE_START);
        try {
            content_length = std::min(
                static_cast<std::size_t>(std::stoull(CL_STR)),
                MAX_BODY_BYTES);
        } catch (const std::exception&) {
            content_length = 0;
        }
    }

    // Body: partial data may already be in raw buffer after headers
    req.body = raw.substr(HEADER_END + HEADER_SEPARATOR.size());

    while (req.body.size() < content_length) {
        const std::size_t NEEDED = content_length - req.body.size();
        ssize_t received = ::recv(sock_fd, buf.data(), std::min(buf.size(), NEEDED), 0);
        if (received <= 0) {
            break;
        }
        req.body.append(buf.data(), static_cast<std::size_t>(received));
    }

    return req;
}

// ---------------------------------------------------------------------------
// HTTP response helpers
// ---------------------------------------------------------------------------

std::string_view http_status_text(HTTP_STATUS status) {
    switch (status) {
        case HTTP_STATUS::OK:                   return "OK";
        case HTTP_STATUS::BAD_REQUEST:          return "Bad Request";
        case HTTP_STATUS::NOT_FOUND:            return "Not Found";
        case HTTP_STATUS::SERVICE_UNAVAILABLE:  return "Service Unavailable";
        default:                                return "Internal Server Error";
    }
}

void send_http_response(int sock_fd, HTTP_STATUS status,
                        std::string_view content_type, const std::string& body) {
    std::string response =
        "HTTP/1.1 " + std::to_string(static_cast<int>(status)) + " "
        + std::string(http_status_text(status)) + "\r\n"
        + "Content-Type: " + std::string(content_type) + "\r\n"
        + "Content-Length: " + std::to_string(body.size()) + "\r\n"
        + "Access-Control-Allow-Origin: *\r\n"
        + "Connection: close\r\n"
        + "\r\n"
        + body;
    send_all(sock_fd, response.data(), response.size());
}

std::string escape_json_string(std::string_view input) {
    std::string escaped;
    escaped.reserve(input.size());

    for (const char ch : input) {
        switch (ch) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(ch); break;
        }
    }

    return escaped;
}

std::string join_messages_as_json_array(const std::vector<std::string>& messages) {
    std::ostringstream oss;
    oss << '[';

    for (std::size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << '"' << escape_json_string(messages[i]) << '"';
    }

    oss << ']';
    return oss.str();
}

// ---------------------------------------------------------------------------
// ROI REST handlers (free functions — do not need class state)
// ---------------------------------------------------------------------------

void handle_roi_get(int sock_fd, const std::string& roi_config_path) {
    if (roi_config_path.empty()) {
        send_http_response(sock_fd, HTTP_STATUS::SERVICE_UNAVAILABLE, "application/json",
                           R"({"error":"ROI config path not set"})");
        return;
    }
    std::ifstream file(roi_config_path);
    if (!file) {
        send_http_response(sock_fd, HTTP_STATUS::INTERNAL_SERVER_ERROR, "application/json",
                           R"({"error":"Failed to read ROI config"})");
        return;
    }
    std::ostringstream content;
    content << file.rdbuf();
    send_http_response(sock_fd, HTTP_STATUS::OK, "application/json", content.str());
}

void handle_roi_put(int sock_fd, const std::string& roi_config_path,
                    const std::string& body) {
    if (roi_config_path.empty()) {
        send_http_response(sock_fd, HTTP_STATUS::SERVICE_UNAVAILABLE, "application/json",
                           R"({"error":"ROI config path not set"})");
        return;
    }
    if (body.empty() || body.front() != '{') {
        send_http_response(sock_fd, HTTP_STATUS::BAD_REQUEST, "application/json",
                           R"({"error":"Invalid JSON body"})");
        return;
    }

    const auto parse_result = catcheye::guard::roi::RoiRepository::from_json_string(body);
    if (!parse_result.success) {
        const std::string error_body =
            std::string(R"({"error":"Invalid ROI config JSON","details":)")
            + join_messages_as_json_array(parse_result.errors) + '}';
        send_http_response(sock_fd, HTTP_STATUS::BAD_REQUEST, "application/json", error_body);
        return;
    }

    const auto validation_result = catcheye::guard::roi::validate_camera_roi_config(parse_result.config);
    if (!validation_result.valid) {
        std::vector<std::string> validation_messages;
        validation_messages.reserve(validation_result.issues.size());

        for (const auto& issue : validation_result.issues) {
            std::ostringstream oss;
            oss << "zone_index=" << issue.zone_index
                << ", point_index=" << issue.point_index
                << ", message=" << issue.message;
            validation_messages.push_back(oss.str());
        }

        const std::string error_body =
            std::string(R"({"error":"Invalid ROI config","details":)")
            + join_messages_as_json_array(validation_messages) + '}';
        send_http_response(sock_fd, HTTP_STATUS::BAD_REQUEST, "application/json", error_body);
        return;
    }

    std::ofstream file(roi_config_path);
    if (!file || !(file << body)) {
        send_http_response(sock_fd, HTTP_STATUS::INTERNAL_SERVER_ERROR, "application/json",
                           R"({"error":"Failed to write ROI config"})");
        return;
    }
    send_http_response(sock_fd, HTTP_STATUS::OK, "application/json", R"({"ok":true})");
}

} // namespace

// ---------------------------------------------------------------------------
// FrameStreamer
// ---------------------------------------------------------------------------

FrameStreamer::FrameStreamer(FrameStreamerConfig config)
    : config_(std::move(config)) {}

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
            logger_ptr->error("frame_streamer: failed to bind port {}: {}",
                              config_.port, std::strerror(errno));
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
        logger_ptr->info("frame_streamer: listening on port {} (ROI API: {})",
                         config_.port,
                         config_.roi_config_path.empty() ? "disabled" : config_.roi_config_path);
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

        // Reduce latency: disable Nagle's algorithm for all clients
        ::setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &SOCKET_ENABLE, sizeof(SOCKET_ENABLE));

        handle_client(client_sock);
    }
}

void FrameStreamer::handle_client(int sock_fd) {
    // Timeout so a stalled client can't block the accept loop indefinitely
    struct timeval recv_timeout {};
    recv_timeout.tv_sec = RECV_TIMEOUT_SEC;
    ::setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    auto req_opt = read_http_request(sock_fd);
    if (!req_opt) {
        ::close(sock_fd);
        return;
    }

    if (req_opt->path == "/") {
        // MJPEG: sock_fd lifetime is managed by handle_mjpeg_connect / send_frame / stop
        handle_mjpeg_connect(sock_fd);
    } else if (req_opt->path == "/api/roi") {
        if (req_opt->method == "GET") {
            handle_roi_get(sock_fd, config_.roi_config_path);
        } else if (req_opt->method == "PUT") {
            handle_roi_put(sock_fd, config_.roi_config_path, req_opt->body);
        } else {
            send_http_response(sock_fd, HTTP_STATUS::BAD_REQUEST, "application/json",
                               R"({"error":"Method not allowed"})");
        }
        ::close(sock_fd);
    } else {
        send_http_response(sock_fd, HTTP_STATUS::NOT_FOUND, "application/json",
                           R"({"error":"Not found"})");
        ::close(sock_fd);
    }
}

void FrameStreamer::handle_mjpeg_connect(int sock_fd) {
    // Tight send timeout: drop frames rather than block on a slow client
    struct timeval send_timeout {};
    send_timeout.tv_usec = SEND_TIMEOUT_US;
    ::setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (static_cast<int>(client_fds_.size()) >= config_.max_clients) {
            if (auto logger_ptr = logger()) {
                logger_ptr->warn("frame_streamer: max clients reached, rejecting connection");
            }
            ::close(sock_fd);
            return;
        }
    }

    if (!send_all(sock_fd, MJPEG_HTTP_HEADER.data(), MJPEG_HTTP_HEADER.size())) {
        ::close(sock_fd);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        client_fds_.push_back(sock_fd);
        if (auto logger_ptr = logger()) {
            logger_ptr->info("frame_streamer: MJPEG client connected (total: {})",
                             client_fds_.size());
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
        client_fds_.erase(
            std::remove(client_fds_.begin(), client_fds_.end(), sock_fd),
            client_fds_.end());
    }

    if (!failed_fds.empty()) {
        if (auto logger_ptr = logger()) {
            logger_ptr->info("frame_streamer: removed {} disconnected client(s), remaining: {}",
                             failed_fds.size(), client_fds_.size());
        }
    }
}

} // namespace catcheye
