#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/core/mat.hpp>

namespace catcheye {

static constexpr int DEFAULT_STREAM_PORT = 8080;

struct FrameStreamerConfig {
    int port = DEFAULT_STREAM_PORT;
    int jpeg_quality = 80;
    int max_clients = 4;
    /// Path to the ROI JSON config file. If set, enables GET /api/roi and PUT /api/roi.
    std::string roi_config_path;
};

/// Streams rendered preview frames to connected clients via MJPEG over HTTP (TCP).
///
/// Endpoints served on the same port:
///   GET  /         → MJPEG stream (multipart/x-mixed-replace)
///   GET  /api/roi  → current ROI config as JSON
///   PUT  /api/roi  → overwrite ROI config JSON (pipeline auto-reloads)
class FrameStreamer {
   public:
    explicit FrameStreamer(FrameStreamerConfig config = {});
    ~FrameStreamer();

    FrameStreamer(const FrameStreamer&) = delete;
    FrameStreamer& operator=(const FrameStreamer&) = delete;

    bool start();
    void stop();
    bool is_running() const;

    /// Encode frame as JPEG and send to all connected clients.
    /// Non-blocking: drops frame if a client can't keep up.
    void send_frame(const cv::Mat& frame);

   private:
    void accept_loop();
    /// Read HTTP request, then route to MJPEG or REST handler.
    void handle_client(int sock_fd);
    /// Send MJPEG HTTP header and add client to the active stream list.
    void handle_mjpeg_connect(int sock_fd);

    FrameStreamerConfig config_;
    int server_fd_ {-1};
    std::atomic<bool> running_ {false};
    std::thread accept_thread_;

    std::mutex clients_mutex_;
    std::vector<int> client_fds_;

    std::vector<std::uint8_t> jpeg_buffer_;
};

} // namespace catcheye
