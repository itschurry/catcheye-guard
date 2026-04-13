#pragma once

#include <vector>

#include "catcheye/guard/detector.hpp"
#include "catcheye/guard/roi/camera_roi_config.hpp"
#include "catcheye/guard/roi/roi_evaluator.hpp"

namespace catcheye {

struct EvaluatedDetection {
    Detection detection;
    catcheye::guard::roi::EvaluationResult roi_result;
};

std::vector<Detection> filter_detections(
    const std::vector<Detection>& detections,
    bool enabled,
    int class_id);

std::vector<EvaluatedDetection> evaluate_detections(
    const std::vector<Detection>& detections,
    bool roi_enabled,
    const catcheye::guard::roi::CameraRoiConfig& roi_config);

} // namespace catcheye
