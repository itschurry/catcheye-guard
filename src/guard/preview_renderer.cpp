#include "guard/preview_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace catcheye {
namespace {

using RoiEvaluationStatus = catcheye::roi::EvaluationStatus;

cv::Rect to_rect(const BoundingBox& box) {
    return cv::Rect(
        static_cast<int>(box.x),
        static_cast<int>(box.y),
        static_cast<int>(box.width),
        static_cast<int>(box.height));
}

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

} // namespace

void draw_roi_zones(cv::Mat& image, const catcheye::roi::CameraRoiConfig& roi_config) {
    for (const bool draw_enabled : {true, false}) {
        const double fill_alpha = draw_enabled ? 0.20 : 0.10;
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
            const cv::Scalar color = draw_enabled ? cv::Scalar(255, 255, 0) : cv::Scalar(128, 128, 128);
            const std::vector<std::vector<cv::Point>> polygons {polygon};
            cv::fillPoly(fill_overlay, polygons, color, cv::LINE_AA);
        }

        if (any_zone_drawn) {
            cv::addWeighted(fill_overlay, fill_alpha, image, 1.0 - fill_alpha, 0.0, image);
        }
    }

    for (const auto& zone : roi_config.allowed_zones) {
        if (zone.points.size() < 2) {
            continue;
        }

        std::vector<cv::Point> polygon;
        polygon.reserve(zone.points.size());
        for (const auto& point : zone.points) {
            polygon.push_back(to_cv_point(point));
        }

        const cv::Scalar color = zone.enabled ? cv::Scalar(255, 255, 0) : cv::Scalar(128, 128, 128);
        const int thickness = zone.enabled ? 2 : 1;

        cv::polylines(image, polygon, true, color, thickness, cv::LINE_AA);

        if (!zone.name.empty()) {
            cv::putText(
                image,
                zone.name,
                polygon.front(),
                cv::FONT_HERSHEY_SIMPLEX,
                0.5,
                color,
                1,
                cv::LINE_AA);
        }
    }
}

void draw_detections(
    cv::Mat& image,
    const std::vector<EvaluatedDetection>& detections,
    const IDetector& detector,
    bool roi_enabled) {
    (void)detector;

    for (const EvaluatedDetection& evaluated_detection : detections) {
        const cv::Rect box = to_rect(evaluated_detection.detection.box);
        const cv::Scalar color = detection_color(evaluated_detection, roi_enabled);
        cv::rectangle(image, box, color, 2);
    }
}

} // namespace catcheye
