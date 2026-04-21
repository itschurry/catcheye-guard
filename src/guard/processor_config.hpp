#pragma once

#include <string>

#include "catcheye/roi/camera_roi_config.hpp"
#include "guard/detector_factory.hpp" // DetectorBackend + DetectorFactoryConfig

namespace catcheye {

struct GuardProcessorConfig {
    // ── 검출 백엔드 설정 ─────────────────────────────────────
    DetectorFactoryConfig detector; // backend 선택 + 각 백엔드별 파라미터 포함

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
