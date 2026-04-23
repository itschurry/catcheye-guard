#pragma once

#include <mutex>
#include <memory>
#include <vector>

#include "catcheye/roi/camera_roi_config.hpp"
#include "catcheye/protocol/frame_message.hpp"
#include "catcheye/runtime/frame_processor.hpp"
#include "guard/detector_interface.hpp" // IDetector
#include "guard/detection_postprocess.hpp"
#include "guard/processor_config.hpp"

namespace catcheye {

class GuardProcessor final : public catcheye::runtime::FrameProcessor {
  public:
    explicit GuardProcessor(GuardProcessorConfig config);

    bool initialize() override;
    catcheye::runtime::ProcessOutput process(const catcheye::input::Frame& frame,
                                             const catcheye::runtime::ProcessContext& context) override;
    bool update_roi_config(const catcheye::roi::CameraRoiConfig& roi_config);

  private:
    struct RoiSnapshot {
        bool enabled = false;
        catcheye::roi::CameraRoiConfig config;
    };

    RoiSnapshot roi_snapshot() const;

    mutable std::mutex roi_mutex_;
    GuardProcessorConfig config_;
    std::unique_ptr<IDetector> detector_;
    std::vector<Detection> cached_detections_;
};

} // namespace catcheye
