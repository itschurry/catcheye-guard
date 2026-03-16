#pragma once

#include <string>

#include "catcheye/guard/roi/camera_roi_config.hpp"
#include "catcheye/guard/roi/point.hpp"

namespace catcheye::guard::roi {

enum class EvaluationStatus {
    Allowed,
    Restricted,
    Invalid
};

struct EvaluationResult {
    EvaluationStatus status {EvaluationStatus::Invalid};
    std::string reason;
};

EvaluationResult evaluate_reference_point(
    const Point& reference_point,
    const CameraRoiConfig& config
);

EvaluationResult evaluate_bbox_bottom_center(
    double x,
    double y,
    double width,
    double height,
    const CameraRoiConfig& config
);

} // namespace catcheye::guard::roi
