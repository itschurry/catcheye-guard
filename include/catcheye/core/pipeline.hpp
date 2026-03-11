#pragma once

#include <string>

#include "catcheye/core/camera.hpp"
#include "catcheye/guard/detector.hpp"

namespace catcheye {

struct PipelineConfig {
    CameraConfig camera;
    DetectorConfig detector;
    std::string window_name = "YOLO26 + NCNN";
    bool render_preview = true;
    bool filter_by_class = true;
    int filter_class_id = 0;
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
