#pragma once

#include <opencv2/core/mat.hpp>

#include "catcheye/roi/camera_roi_config.hpp"
#include "guard/detection_postprocess.hpp"
#include "catcheye/detection/detector.hpp"
#include "catcheye/visualization/annotation_renderer.hpp"

namespace catcheye {

void draw_roi_zones(cv::Mat& image, const catcheye::roi::CameraRoiConfig& roi_config);
void draw_pallet_roi_zones(cv::Mat& image, const catcheye::roi::CameraRoiConfig& roi_config);

void draw_detections(cv::Mat& image, const std::vector<EvaluatedDetection>& detections, const IDetector& detector, bool roi_enabled);
void draw_pallet_detections(cv::Mat& image, const std::vector<PalletEvaluation>& detections);

} // namespace catcheye
