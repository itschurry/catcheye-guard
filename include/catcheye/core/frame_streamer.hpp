#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include <opencv2/core/mat.hpp>

namespace catcheye {

static constexpr int DEFAULT_STREAM_PORT = 8080;

struct FrameStreamerConfig {
    int port = DEFAULT_STREAM_PORT;
    int jpeg_quality = 80;
    int max_clients = 4;
};

/// Streams rendered preview frames to connected clients via MJPEG over HTTP (TCP).
/// Clients connect to http://<host>:<port>/ and receive a multipart/x-mixed-replace stream.
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

    FrameStreamerConfig config_;
    int server_fd_ {-1};
    std::atomic<bool> running_ {false};
    std::thread accept_thread_;

    std::mutex clients_mutex_;
    std::vector<int> client_fds_;

    std::vector<std::uint8_t> jpeg_buffer_;
};

} // namespace catcheye
