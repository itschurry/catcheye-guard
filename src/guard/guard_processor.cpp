#include "guard/guard_processor.hpp"

#include <cstdint>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "catcheye/protocol/frame_message.hpp"
#include "catcheye/guard/roi/roi_validation.hpp"
#include "catcheye/utils/logger.hpp"
#include "guard/preview_renderer.hpp"
#include "guard/roi_reload.hpp"

namespace catcheye {
namespace {

using RoiEvaluationStatus = catcheye::guard::roi::EvaluationStatus;
constexpr std::uint64_t ROI_RELOAD_INTERVAL_FRAMES = 15;

std::string roi_status_to_string(RoiEvaluationStatus status)
{
    switch (status) {
        case RoiEvaluationStatus::Allowed:
            return "allowed";
        case RoiEvaluationStatus::Restricted:
            return "restricted";
        case RoiEvaluationStatus::Invalid:
            return "invalid";
    }
    return "unknown";
}

std::string escape_json(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

std::string build_metadata_json(
    const std::vector<EvaluatedDetection>& detections,
    const Detector& detector,
    bool roi_enabled)
{
    std::ostringstream oss;
    oss << "{\"roi_enabled\":" << (roi_enabled ? "true" : "false")
        << ",\"detection_count\":" << detections.size()
        << ",\"detections\":[";

    for (std::size_t i = 0; i < detections.size(); ++i) {
        const auto& item = detections[i];
        if (i > 0) {
            oss << ',';
        }
        oss << "{"
            << "\"class_id\":" << item.detection.class_id
            << ",\"class_name\":\"" << escape_json(detector.class_name(item.detection.class_id)) << "\""
            << ",\"score\":" << item.detection.score
            << ",\"bbox\":{"
            << "\"x\":" << item.detection.box.x
            << ",\"y\":" << item.detection.box.y
            << ",\"width\":" << item.detection.box.width
            << ",\"height\":" << item.detection.box.height
            << "}"
            << ",\"roi_status\":\"" << roi_status_to_string(item.roi_result.status) << "\""
            << ",\"roi_reason\":\"" << escape_json(item.roi_result.reason) << "\""
            << "}";
    }

    oss << "]}";
    return oss.str();
}

} // namespace

GuardProcessor::GuardProcessor(GuardProcessorConfig config)
    : config_(std::move(config)),
      detector_(config_.detector) {}

bool GuardProcessor::initialize()
{
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
            return false;
        }
    }

    initialize_roi_reload_watch(config_, last_seen_roi_config_text_, roi_reload_watch_warning_emitted_);

    if (!detector_.initialize()) {
        if (const auto log = logger()) {
            log->error("failed to initialize detector");
        }
        return false;
    }

    return true;
}

catcheye::runtime::ProcessOutput GuardProcessor::process(
    const catcheye::input::Frame& frame,
    const catcheye::runtime::ProcessContext& context)
{
    if (context.frame_index % ROI_RELOAD_INTERVAL_FRAMES == 0U) {
        try_reload_roi_config(config_, last_seen_roi_config_text_, roi_reload_watch_warning_emitted_);
    }

    if (context.should_process) {
        cached_detections_ = detector_.detect(frame);
    }

    const std::vector<Detection> visible_detections = filter_detections(
        cached_detections_,
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
                context.frame_index,
                evaluated_detections.size());
        }
    }

    if (config_.roi_enabled) {
        for (const EvaluatedDetection& evaluated_detection : evaluated_detections) {
            if (evaluated_detection.roi_result.status == RoiEvaluationStatus::Restricted) {
                if (const auto log = logger()) {
                    log->warn(
                        "ROI restricted detection on frame {}: class_id={}, score={:.2f}, reason={}",
                        context.frame_index,
                        evaluated_detection.detection.class_id,
                        evaluated_detection.detection.score,
                        evaluated_detection.roi_result.reason);
                }
            } else if (evaluated_detection.roi_result.status == RoiEvaluationStatus::Invalid) {
                if (const auto log = logger()) {
                    log->warn(
                        "ROI invalid evaluation on frame {}: class_id={}, reason={}",
                        context.frame_index,
                        evaluated_detection.detection.class_id,
                        evaluated_detection.roi_result.reason);
                }
            }
        }
    }

    catcheye::runtime::ProcessOutput output;
    if (!context.needs_preview && !context.needs_publish) {
        return output;
    }

    cv::Mat preview = frame.image.clone();
    if (config_.roi_enabled) {
        if (frame.width() != config_.roi_config.image_width
            || frame.height() != config_.roi_config.image_height) {
            if (!roi_frame_size_warning_emitted_) {
                roi_frame_size_warning_emitted_ = true;
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
            roi_frame_size_warning_emitted_ = false;
        }

        draw_roi_zones(preview, config_.roi_config);
    }
    draw_detections(preview, evaluated_detections, detector_, config_.roi_enabled);

    if (context.needs_preview) {
        output.has_preview = true;
        output.preview_frame = preview.clone();
    }

    if (context.needs_publish) {
        output.has_message = true;
        output.message = catcheye::protocol::encode_jpeg_frame(
            preview,
            build_metadata_json(evaluated_detections, detector_, config_.roi_enabled),
            "person-guard",
            jpeg_quality_);
    }

    return output;
}

} // namespace catcheye
