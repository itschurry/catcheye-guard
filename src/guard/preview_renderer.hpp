#pragma once

#include <opencv2/core/mat.hpp>

#include "guard/detection_postprocess.hpp"

namespace catcheye {

void draw_roi_zones(cv::Mat& image, const catcheye::guard::roi::CameraRoiConfig& roi_config);

void draw_detections(
    cv::Mat& image,
    const std::vector<EvaluatedDetection>& detections,
    const Detector& detector,
    bool roi_enabled);

} // namespace catcheye
