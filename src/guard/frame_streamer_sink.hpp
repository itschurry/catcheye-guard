#pragma once

#include <memory>

#include "catcheye/core/frame_streamer.hpp"
#include "catcheye/runtime/preview_sink.hpp"

namespace catcheye {

class FrameStreamerSink final : public catcheye::runtime::PreviewSink {
   public:
    explicit FrameStreamerSink(FrameStreamerConfig config);

    bool start() override;
    void stop() override;
    void publish(const cv::Mat& frame) override;

   private:
    FrameStreamer streamer_;
};

} // namespace catcheye
