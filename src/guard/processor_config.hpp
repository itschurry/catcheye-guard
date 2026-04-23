#pragma once

#include <string>

#include "catcheye/hardware/gpio_signal_config.hpp"
#include "catcheye/roi/camera_roi_config.hpp"
#include "guard/detector_factory.hpp"

namespace catcheye {

struct GuardProcessorConfig {
    DetectorFactoryConfig detector;

    // ── 클래스 필터 ──────────────────────────────────────────
    bool filter_by_class = true;
    int filter_class_id = 0; // 0 = person (COCO)

    // ── ROI ──────────────────────────────────────────────────
    bool roi_enabled = false;
    std::string roi_config_path;
    catcheye::roi::CameraRoiConfig roi_config;

    // ── GPIO signal ──────────────────────────────────────────
    GpioSignalConfig roi_alert_gpio;
};

} // namespace catcheye
