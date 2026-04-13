#include "guard/guard_processor.hpp"

#include <cstdint>
#include <utility>
#include <vector>

#include "catcheye/guard/roi/roi_validation.hpp"
#include "catcheye/utils/logger.hpp"
#include "guard/preview_renderer.hpp"
#include "guard/roi_reload.hpp"

namespace catcheye {
namespace {

using RoiEvaluationStatus = catcheye::guard::roi::EvaluationStatus;
constexpr std::uint64_t ROI_RELOAD_INTERVAL_FRAMES = 15;

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
    if (!context.needs_visualization) {
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

    output.has_visualization = true;
    output.visualization = std::move(preview);
    return output;
}

} // namespace catcheye
