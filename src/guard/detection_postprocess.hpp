#pragma once

#include <vector>

#include "guard/detector.hpp"
#include "catcheye/roi/camera_roi_config.hpp"
#include "catcheye/roi/roi_evaluator.hpp"

namespace catcheye {

struct EvaluatedDetection {
    Detection detection;
    catcheye::roi::EvaluationResult roi_result;
};

std::vector<Detection> filter_detections(
    const std::vector<Detection>& detections,
    bool enabled,
    int class_id);

std::vector<EvaluatedDetection> evaluate_detections(
    const std::vector<Detection>& detections,
    bool roi_enabled,
    const catcheye::roi::CameraRoiConfig& roi_config);

} // namespace catcheye
