#include "catcheye/core/pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "catcheye/guard/roi/roi_evaluator.hpp"
#include "catcheye/guard/roi/roi_repository.hpp"
#include "catcheye/guard/roi/roi_validation.hpp"
#include "catcheye/utils/logger.hpp"

namespace catcheye {
namespace {

using RoiConfig = catcheye::guard::roi::CameraRoiConfig;
using RoiEvaluationResult = catcheye::guard::roi::EvaluationResult;
using RoiEvaluationStatus = catcheye::guard::roi::EvaluationStatus;

cv::Rect to_rect(const BoundingBox& box) {
    return cv::Rect(
        static_cast<int>(box.x),
        static_cast<int>(box.y),
        static_cast<int>(box.width),
        static_cast<int>(box.height));
}

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

struct EvaluatedDetection {
    Detection detection;
    RoiEvaluationResult roi_result;
};

cv::Point to_cv_point(const catcheye::guard::roi::Point& point) {
    return cv::Point(
        static_cast<int>(std::lround(point.x)),
        static_cast<int>(std::lround(point.y)));
}

RoiEvaluationResult evaluate_detection_roi(const Detection& detection, const RoiConfig& roi_config) {
    return catcheye::guard::roi::evaluate_bbox_fully_inside(
        static_cast<double>(detection.box.x),
        static_cast<double>(detection.box.y),
        static_cast<double>(detection.box.width),
        static_cast<double>(detection.box.height),
        roi_config);
}

std::vector<EvaluatedDetection> evaluate_detections(
    const std::vector<Detection>& detections,
    bool roi_enabled,
    const RoiConfig& roi_config) {
    std::vector<EvaluatedDetection> evaluated;
    evaluated.reserve(detections.size());

    for (const Detection& detection : detections) {
        RoiEvaluationResult roi_result;
        if (roi_enabled) {
            roi_result = evaluate_detection_roi(detection, roi_config);
        } else {
            roi_result = {RoiEvaluationStatus::Allowed, "ROI disabled"};
        }
        evaluated.push_back(EvaluatedDetection {detection, std::move(roi_result)});
    }

    return evaluated;
}

bool read_text_file(const std::string& path, std::string& contents, std::string& error_message) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        error_message = "failed to open file";
        return false;
    }

    contents.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    if (!ifs.good() && !ifs.eof()) {
        error_message = "failed while reading file";
        return false;
    }

    error_message.clear();
    return true;
}

bool try_reload_roi_config(
    PipelineConfig& config,
    std::string& last_seen_roi_config_text,
    bool& roi_reload_watch_warning_emitted) {
    if (!config.roi_enabled || !config.roi_auto_reload || config.roi_config_path.empty()) {
        return false;
    }

    std::string current_text;
    std::string error_message;
    if (!read_text_file(config.roi_config_path, current_text, error_message)) {
        if (!roi_reload_watch_warning_emitted) {
            roi_reload_watch_warning_emitted = true;
            if (const auto log = logger()) {
                log->warn(
                    "failed to read ROI config '{}': {}",
                    config.roi_config_path,
                    error_message);
            }
        }
        return false;
    }

    roi_reload_watch_warning_emitted = false;
    if (current_text == last_seen_roi_config_text) {
        return false;
    }

    last_seen_roi_config_text = current_text;

    if (const auto log = logger()) {
        log->info("detected ROI config change, reloading '{}'", config.roi_config_path);
    }

    const auto parse_result = catcheye::guard::roi::RoiRepository::from_json_string(current_text);
    if (!parse_result.success) {
        if (const auto log = logger()) {
            log->warn("ROI reload failed, keeping previous config");
            for (const std::string& error : parse_result.errors) {
                log->warn("ROI parse error: {}", error);
            }
        }
        return false;
    }

    const auto validation_result = catcheye::guard::roi::validate_camera_roi_config(parse_result.config);
    if (!validation_result.valid) {
        if (const auto log = logger()) {
            log->warn("reloaded ROI config is invalid, keeping previous config");
            for (const auto& issue : validation_result.issues) {
                log->warn(
                    "ROI validation issue: zone_index={}, point_index={}, message={}",
                    issue.zone_index,
                    issue.point_index,
                    issue.message);
            }
        }
        return false;
    }

    config.roi_config = parse_result.config;
    if (const auto log = logger()) {
        log->info(
            "ROI config reloaded successfully: camera_id='{}', zones={}",
            config.roi_config.camera_id,
            config.roi_config.allowed_zones.size());
    }
    return true;
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

std::string roi_status_text(const EvaluatedDetection& detection, bool roi_enabled) {
    if (!roi_enabled) {
        return "ROI OFF";
    }

    switch (detection.roi_result.status) {
        case RoiEvaluationStatus::Allowed:
            return "ALLOWED";
        case RoiEvaluationStatus::Restricted:
            return "RESTRICTED";
        case RoiEvaluationStatus::Invalid:
            return "ROI INVALID";
    }

    return "ROI UNKNOWN";
}

void draw_roi_zones(cv::Mat& image, const RoiConfig& roi_config) {
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
        const double fill_alpha = zone.enabled ? 0.20 : 0.10;
        const int thickness = zone.enabled ? 2 : 1;

        cv::Mat fill_overlay = image.clone();
        const std::vector<std::vector<cv::Point>> polygons {polygon};
        cv::fillPoly(fill_overlay, polygons, color, cv::LINE_AA);
        cv::addWeighted(fill_overlay, fill_alpha, image, 1.0 - fill_alpha, 0.0, image);

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
    const Detector& detector,
    bool roi_enabled) {
    for (const EvaluatedDetection& evaluated_detection : detections) {
        const cv::Rect box = to_rect(evaluated_detection.detection.box);
        const cv::Scalar color = detection_color(evaluated_detection, roi_enabled);
        cv::rectangle(image, box, color, 2);

        const std::string label =
            detector.class_name(evaluated_detection.detection.class_id)
            + " "
            + cv::format("%.2f", evaluated_detection.detection.score)
            + " "
            + roi_status_text(evaluated_detection, roi_enabled);

        int baseline = 0;
        const cv::Size text_size = cv::getTextSize(
            label,
            cv::FONT_HERSHEY_SIMPLEX,
            0.5,
            1,
            &baseline);

        const int label_x = std::max(box.x, 0);
        const int label_y = std::max(box.y - 4, text_size.height + 4);
        const cv::Rect background(
            label_x,
            label_y - text_size.height - 6,
            text_size.width + 6,
            text_size.height + 6);

        cv::rectangle(image, background, color, cv::FILLED);
        cv::putText(
            image,
            label,
            cv::Point(label_x + 3, label_y - 3),
            cv::FONT_HERSHEY_SIMPLEX,
            0.5,
            cv::Scalar(0, 0, 0),
            1);

    }
}

} // namespace

Pipeline::Pipeline(PipelineConfig config)
    : config_(std::move(config)),
      camera_(config_.camera),
      detector_(config_.detector) {
    if (config_.stream_preview) {
        frame_streamer_ = std::make_unique<FrameStreamer>(config_.stream_config);
    }
}

int Pipeline::run() {
    if (const auto log = logger()) {
        log->info(
            "starting pipeline, preview={}, filter_by_class={}, filter_class_id={}, roi_enabled={}",
            config_.render_preview,
            config_.filter_by_class,
            config_.filter_class_id,
            config_.roi_enabled);
    }

    if (config_.roi_enabled) {
        const auto validation = catcheye::guard::roi::validate_camera_roi_config(config_.roi_config);
        if (!validation.valid) {
            if (const auto log = logger()) {
                log->error("ROI config is invalid with {} issue(s)", validation.issues.size());
                for (const auto& issue : validation.issues) {
                    log->error(
                        "ROI issue: zone_index={}, point_index={}, message={}",
                        issue.zone_index,
                        issue.point_index,
                        issue.message);
                }
            }
            return 1;
        }
    }

    std::string last_seen_roi_config_text;
    bool roi_reload_watch_warning_emitted = false;
    if (config_.roi_enabled && config_.roi_auto_reload && !config_.roi_config_path.empty()) {
        std::string error_message;
        if (!read_text_file(config_.roi_config_path, last_seen_roi_config_text, error_message)) {
            roi_reload_watch_warning_emitted = true;
            if (const auto log = logger()) {
                log->warn(
                    "failed to initialize ROI file watch for '{}': {}",
                    config_.roi_config_path,
                    error_message);
            }
        } else if (const auto log = logger()) {
            log->info("ROI auto-reload watching '{}'", config_.roi_config_path);
        }
    }

    if (!camera_.open()) {
        if (const auto log = logger()) {
            log->error("failed to open camera pipeline");
        }
        return 1;
    }

    if (!detector_.initialize()) {
        if (const auto log = logger()) {
            log->error("failed to initialize detector");
        }
        camera_.close();
        return 1;
    }

    if (config_.stream_preview && frame_streamer_) {
        if (!frame_streamer_->start()) {
            if (const auto log = logger()) {
                log->error("failed to start frame streamer");
            }
            camera_.close();
            return 1;
        }
    }

    Frame frame;
    std::uint64_t frame_count = 0;
    bool roi_frame_size_warning_emitted = false;
    while (true) {
        if (!camera_.read(frame)) {
            if (const auto log = logger()) {
                log->warn("stopping pipeline because frame read failed");
            }
            break;
        }
        ++frame_count;

        try_reload_roi_config(config_, last_seen_roi_config_text, roi_reload_watch_warning_emitted);

        const std::vector<Detection> detections = detector_.detect(frame);
        const std::vector<Detection> visible_detections = filter_detections(
            detections,
            config_.filter_by_class,
            config_.filter_class_id);
        const std::vector<EvaluatedDetection> evaluated_detections = evaluate_detections(
            visible_detections,
            config_.roi_enabled,
            config_.roi_config);

        if (!evaluated_detections.empty()) {
            if (const auto log = logger()) {
                log->debug(
                    "frame {} produced {} visible detections",
                    frame_count,
                    evaluated_detections.size());
            }
        }

        if (config_.roi_enabled) {
            for (const EvaluatedDetection& evaluated_detection : evaluated_detections) {
                if (evaluated_detection.roi_result.status == RoiEvaluationStatus::Restricted) {
                    if (const auto log = logger()) {
                        log->warn(
                            "ROI restricted detection on frame {}: class_id={}, score={:.2f}, reason={}",
                            frame_count,
                            evaluated_detection.detection.class_id,
                            evaluated_detection.detection.score,
                            evaluated_detection.roi_result.reason);
                    }
                } else if (evaluated_detection.roi_result.status == RoiEvaluationStatus::Invalid) {
                    if (const auto log = logger()) {
                        log->warn(
                            "ROI invalid evaluation on frame {}: class_id={}, reason={}",
                            frame_count,
                            evaluated_detection.detection.class_id,
                            evaluated_detection.roi_result.reason);
                    }
                }
            }
        }

        if (config_.render_preview || config_.stream_preview) {
            cv::Mat preview = frame.image.clone();
            if (config_.roi_enabled) {
                if (frame.width() != config_.roi_config.image_width
                    || frame.height() != config_.roi_config.image_height) {
                    if (!roi_frame_size_warning_emitted) {
                        roi_frame_size_warning_emitted = true;
                        if (const auto log = logger()) {
                            log->warn(
                                "frame size {}x{} does not match ROI config {}x{}",
                                frame.width(),
                                frame.height(),
                                config_.roi_config.image_width,
                                config_.roi_config.image_height);
                        }
                    }
                } else {
                    roi_frame_size_warning_emitted = false;
                }

                draw_roi_zones(preview, config_.roi_config);
            }
            draw_detections(preview, evaluated_detections, detector_, config_.roi_enabled);

            if (config_.stream_preview && frame_streamer_) {
                frame_streamer_->send_frame(preview);
            }

            if (config_.render_preview) {
                cv::imshow(config_.window_name, preview);
                const int key = cv::waitKey(1);
                if (key == 27 || key == 'q') {
                    if (const auto log = logger()) {
                        log->info("pipeline interrupted by user input '{}'", key);
                    }
                    break;
                }
            }
        }
    }

    camera_.close();
    if (frame_streamer_) {
        frame_streamer_->stop();
    }
    cv::destroyAllWindows();
    if (const auto log = logger()) {
        log->info("pipeline stopped after {} frames", frame_count);
    }
    return 0;
}

} // namespace catcheye
