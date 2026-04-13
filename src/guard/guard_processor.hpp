#pragma once

#include <string>
#include <vector>

#include "catcheye/protocol/frame_message.hpp"
#include "catcheye/runtime/frame_processor.hpp"
#include "guard/guard_processor_config.hpp"
#include "guard/detection_postprocess.hpp"

namespace catcheye {

class GuardProcessor final : public catcheye::runtime::FrameProcessor {
   public:
    explicit GuardProcessor(GuardProcessorConfig config);

    bool initialize() override;
    catcheye::runtime::ProcessOutput process(
        const catcheye::input::Frame& frame,
        const catcheye::runtime::ProcessContext& context) override;

   private:
    GuardProcessorConfig config_;
    Detector detector_;
    std::vector<Detection> cached_detections_;
    std::string last_seen_roi_config_text_;
    bool roi_reload_watch_warning_emitted_ = false;
    bool roi_frame_size_warning_emitted_ = false;
    int jpeg_quality_ = 80;
};

} // namespace catcheye
