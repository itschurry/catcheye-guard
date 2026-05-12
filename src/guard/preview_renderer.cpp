#include "guard/preview_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace catcheye {
namespace {

using RoiEvaluationStatus = catcheye::roi::EvaluationStatus;
const cv::Scalar kEnabledRoiColor(0, 215, 255);
const cv::Scalar kDisabledRoiColor(128, 128, 128);
const cv::Scalar kPalletRoiColor(255, 255, 255);
const cv::Scalar kPalletPresentColor(128, 128, 128);
const cv::Scalar kPalletOutsideColor(255, 0, 255);

cv::Point to_cv_point(const catcheye::roi::Point& point) {
    return cv::Point(
        static_cast<int>(std::lround(point.x)),
        static_cast<int>(std::lround(point.y)));
}

cv::Scalar detection_color(const EvaluatedDetection& detection, bool roi_enabled) {
    if (!roi_enabled) {
        return cv::Scalar(0, 255, 0);
    }

    switch (detection.roi_result.status) {
        case RoiEvaluationStatus::Allowed:
            return cv::Scalar(0, 200, 0);
        case RoiEvaluationStatus::Restricted:
            return cv::Scalar(0, 0, 255);
        case RoiEvaluationStatus::Invalid:
            return cv::Scalar(0, 215, 255);
    }

    return cv::Scalar(255, 255, 255);
}

catcheye::visualization::DetectionAnnotationStyle annotation_style(cv::Scalar color) {
    return catcheye::visualization::DetectionAnnotationStyle{
        .box_color = color,
        .line_thickness = 2,
    };
}

} // namespace

void draw_zones(cv::Mat& image, const catcheye::roi::CameraRoiConfig& roi_config, cv::Scalar enabled_color, cv::Scalar disabled_color) {
    for (const bool draw_enabled : {true, false}) {
        const double fill_alpha = draw_enabled ? 0.18 : 0.10;
        cv::Mat fill_overlay;
        bool any_zone_drawn = false;

        for (const auto& zone : roi_config.allowed_zones) {
            if (zone.enabled != draw_enabled || zone.points.size() < 2) {
                continue;
            }
            if (!any_zone_drawn) {
                fill_overlay = image.clone();
                any_zone_drawn = true;
            }
            std::vector<cv::Point> polygon;
            polygon.reserve(zone.points.size());
            for (const auto& point : zone.points) {
                polygon.push_back(to_cv_point(point));
            }
            const cv::Scalar color = draw_enabled ? enabled_color : disabled_color;
            const std::vector<std::vector<cv::Point>> polygons {polygon};
            cv::fillPoly(fill_overlay, polygons, color, cv::LINE_AA);
        }

        if (any_zone_drawn) {
            cv::addWeighted(fill_overlay, fill_alpha, image, 1.0 - fill_alpha, 0.0, image);
        }
    }
}

void draw_roi_zones(cv::Mat& image, const catcheye::roi::CameraRoiConfig& roi_config) {
    draw_zones(image, roi_config, kEnabledRoiColor, kDisabledRoiColor);
}

void draw_pallet_roi_zones(cv::Mat& image, const catcheye::roi::CameraRoiConfig& roi_config) {
    draw_zones(image, roi_config, kPalletRoiColor, kDisabledRoiColor);
}

void draw_detections(
    cv::Mat& image,
    const std::vector<EvaluatedDetection>& detections,
    const IDetector& detector,
    bool roi_enabled) {
    (void)detector;

    for (const EvaluatedDetection& evaluated_detection : detections) {
        const cv::Scalar color = detection_color(evaluated_detection, roi_enabled);
        const std::vector<Detection> detection{evaluated_detection.detection};
        catcheye::visualization::draw_detection_boxes(image, detection, annotation_style(color));
    }
}

void draw_pallet_detections(cv::Mat& image, const std::vector<PalletEvaluation>& detections) {
    for (const PalletEvaluation& detection : detections) {
        const cv::Scalar color = detection.present ? kPalletPresentColor : kPalletOutsideColor;
        const std::vector<Detection> single_detection{detection.detection};
        catcheye::visualization::draw_detection_boxes(image, single_detection, annotation_style(color));
    }
}

} // namespace catcheye
