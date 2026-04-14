#pragma once

#include <string>

#include "catcheye/guard/detector.hpp"
#include "catcheye/roi/camera_roi_config.hpp"

namespace catcheye {

struct GuardProcessorConfig {
    DetectorConfig detector;
    bool filter_by_class = true;
    int filter_class_id = 0;
    bool roi_enabled = false;
    bool roi_auto_reload = true;
    std::string roi_config_path;
    catcheye::roi::CameraRoiConfig roi_config;
};

} // namespace catcheye
