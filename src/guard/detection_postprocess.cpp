#include "guard/detection_postprocess.hpp"

namespace catcheye {
namespace {

catcheye::roi::EvaluationResult evaluate_detection_roi(
    const Detection& detection,
    const catcheye::roi::CameraRoiConfig& roi_config) {
    auto result = catcheye::roi::evaluate_bbox_intersects(
        static_cast<double>(detection.box.x),
        static_cast<double>(detection.box.y),
        static_cast<double>(detection.box.width),
        static_cast<double>(detection.box.height),
        roi_config);

    if (result.status == catcheye::roi::EvaluationStatus::Allowed) {
        return {catcheye::roi::EvaluationStatus::Restricted, "person intersects a danger zone"};
    }
    if (result.status == catcheye::roi::EvaluationStatus::Restricted) {
        return {catcheye::roi::EvaluationStatus::Allowed, "person is outside all danger zones"};
    }
    return result;
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

std::vector<PalletEvaluation> evaluate_pallet_detections(
    const std::vector<Detection>& detections,
    bool roi_enabled,
    const catcheye::roi::CameraRoiConfig& roi_config) {
    std::vector<PalletEvaluation> evaluated;
    evaluated.reserve(detections.size());

    for (const Detection& detection : detections) {
        catcheye::roi::EvaluationResult roi_result;
        if (roi_enabled) {
            roi_result = catcheye::roi::evaluate_bbox_fully_inside(
                static_cast<double>(detection.box.x),
                static_cast<double>(detection.box.y),
                static_cast<double>(detection.box.width),
                static_cast<double>(detection.box.height),
                roi_config);
        } else {
            roi_result = {catcheye::roi::EvaluationStatus::Invalid, "pallet ROI disabled"};
        }

        const bool present = roi_result.status == catcheye::roi::EvaluationStatus::Allowed;
        evaluated.push_back(PalletEvaluation {detection, std::move(roi_result), present});
    }

    return evaluated;
}

} // namespace catcheye
