#include "guard/detection_postprocess.hpp"

namespace catcheye {
namespace {

catcheye::roi::EvaluationResult evaluate_detection_roi(
    const Detection& detection,
    const catcheye::roi::CameraRoiConfig& roi_config) {
    return catcheye::roi::evaluate_bbox_fully_inside(
        static_cast<double>(detection.box.x),
        static_cast<double>(detection.box.y),
        static_cast<double>(detection.box.width),
        static_cast<double>(detection.box.height),
        roi_config);
}

} // namespace

std::vector<Detection> filter_detections(
    const std::vector<Detection>& detections,
    bool enabled,
    int class_id) {
    if (!enabled) {
        return detections;
    }

    std::vector<Detection> filtered;
    filtered.reserve(detections.size());
    for (const Detection& detection : detections) {
        if (detection.class_id == class_id) {
            filtered.push_back(detection);
        }
    }

    return filtered;
}

std::vector<EvaluatedDetection> evaluate_detections(
    const std::vector<Detection>& detections,
    bool roi_enabled,
    const catcheye::roi::CameraRoiConfig& roi_config) {
    std::vector<EvaluatedDetection> evaluated;
    evaluated.reserve(detections.size());

    for (const Detection& detection : detections) {
        catcheye::roi::EvaluationResult roi_result;
        if (roi_enabled) {
            roi_result = evaluate_detection_roi(detection, roi_config);
        } else {
            roi_result = {catcheye::roi::EvaluationStatus::Allowed, "ROI disabled"};
        }
        evaluated.push_back(EvaluatedDetection {detection, std::move(roi_result)});
    }

    return evaluated;
}

} // namespace catcheye
