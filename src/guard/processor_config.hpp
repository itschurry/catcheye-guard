#pragma once

#include <cstdint>
#include <string>

#include "catcheye/hardware/gpio_signal_config.hpp"
#include "catcheye/roi/camera_roi_config.hpp"
#include "guard/detector_factory.hpp"

namespace catcheye {

struct GuardProcessorConfig {
    bool detection_enabled = true;
    DetectorFactoryConfig detector;

    // ── 클래스 필터 ──────────────────────────────────────────
    bool filter_by_class = true;
    int filter_class_id = 0; // 0 = person
    int pallet_class_id = 1; // 1 = pallet

    // ── ROI ──────────────────────────────────────────────────
    bool roi_enabled = false;
    std::string roi_config_path;
    catcheye::roi::CameraRoiConfig roi_config;
    std::uint64_t roi_restricted_log_interval_frames = 100;

    // ── Pallet ROI ───────────────────────────────────────────
    bool pallet_detection_enabled = true;
    bool pallet_roi_enabled = false;
    std::string pallet_roi_config_path;
    catcheye::roi::CameraRoiConfig pallet_roi_config;

    // ── GPIO signal ──────────────────────────────────────────
    GpioSignalConfig roi_alert_gpio;
};

} // namespace catcheye
