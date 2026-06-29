#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "catcheye/hardware/gpio_signal.hpp"
#include "catcheye/roi/camera_roi_config.hpp"
#include "catcheye/protocol/frame_message.hpp"
#include "catcheye/runtime/frame_processor.hpp"
#include "catcheye/detection/detector.hpp"
#include "guard/detection_postprocess.hpp"
#include "guard/processor_config.hpp"

namespace catcheye {

struct RoiAlertRuntimeStatus {
    bool person_roi_alert_disabled = false;
    bool roi_alert_output_active = false;
};

class GuardProcessor final : public catcheye::runtime::FrameProcessor {
  public:
    explicit GuardProcessor(GuardProcessorConfig config);

    bool initialize() override;
    catcheye::runtime::ProcessOutput process(const catcheye::input::Frame& frame,
                                             const catcheye::runtime::ProcessContext& context) override;
    bool update_roi_config(const catcheye::roi::CameraRoiConfig& roi_config);
    bool update_pallet_roi_config(const catcheye::roi::CameraRoiConfig& roi_config);
    RoiAlertRuntimeStatus roi_alert_status() const;
    std::string roi_alert_status_json() const;

  private:
    struct RoiSnapshot {
        bool enabled = false;
        catcheye::roi::CameraRoiConfig config;
    };

    RoiSnapshot roi_snapshot() const;
    RoiSnapshot pallet_roi_snapshot() const;
    void set_person_roi_alert_disabled(bool disabled);
    void apply_roi_alert_output_locked(std::optional<std::uint64_t> frame_index);

    mutable std::mutex roi_mutex_;
    mutable std::mutex alert_mutex_;
    GuardProcessorConfig config_;
    std::unique_ptr<IDetector> detector_;
    std::unique_ptr<catcheye::hardware::GpioStateSignal> roi_alert_signal_;
    std::unique_ptr<catcheye::hardware::GpioInputSignal> person_roi_alert_disable_signal_;
    std::vector<Detection> cached_detections_;
    double cached_inference_ms_ = 0.0;
    bool person_roi_alert_disabled_ = false;
    bool person_roi_violation_active_ = false;
    bool pallet_alert_active_ = false;
    bool roi_alert_output_active_ = false;
    std::uint64_t last_roi_restricted_log_frame_ = 0;
    bool has_logged_roi_restricted_ = false;
};

} // namespace catcheye
