#include "guard/frame_streamer_sink.hpp"

#include <utility>

namespace catcheye {

FrameStreamerSink::FrameStreamerSink(FrameStreamerConfig config)
    : streamer_(std::move(config)) {}

bool FrameStreamerSink::start()
{
    return streamer_.start();
}

void FrameStreamerSink::stop()
{
    streamer_.stop();
}

void FrameStreamerSink::publish(const cv::Mat& frame)
{
    streamer_.send_frame(frame);
}

} // namespace catcheye
