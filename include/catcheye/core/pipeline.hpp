#pragma once

#include <string>

#include "catcheye/core/camera.hpp"
#include "catcheye/guard/detector.hpp"
#include "catcheye/guard/roi/camera_roi_config.hpp"

namespace catcheye {

struct PipelineConfig {
    CameraConfig camera;
    DetectorConfig detector;
    std::string window_name = "YOLO26 + NCNN";
    bool render_preview = true;
    bool filter_by_class = true;
    int filter_class_id = 0;
    bool roi_enabled = false;
    bool roi_auto_reload = true;
    std::string roi_config_path;
    catcheye::guard::roi::CameraRoiConfig roi_config;
};

class Pipeline {
   public:
    explicit Pipeline(PipelineConfig config);

    int run();

   private:
    PipelineConfig config_;
    Camera camera_;
    Detector detector_;
};

} // namespace catcheye
