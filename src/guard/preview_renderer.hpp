#pragma once

#include <opencv2/core/mat.hpp>

#include "catcheye/roi/camera_roi_config.hpp"
#include "guard/detection_postprocess.hpp"
#include "guard/detector_interface.hpp" // IDetector (Detector 대신)

namespace catcheye {

void draw_roi_zones(cv::Mat& image, const catcheye::roi::CameraRoiConfig& roi_config);

// Detector& → IDetector& 로 변경 (구현 파일은 수정 불필요)
void draw_detections(cv::Mat& image, const std::vector<EvaluatedDetection>& detections, const IDetector& detector, bool roi_enabled);

} // namespace catcheye
