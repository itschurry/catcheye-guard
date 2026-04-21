#pragma once

#include <string>

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
    bool roi_auto_reload = true;
    std::string roi_config_path;
    catcheye::roi::CameraRoiConfig roi_config;
};

} // namespace catcheye
