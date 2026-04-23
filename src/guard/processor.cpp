#include "guard/processor.hpp"

#include <cstdint>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "catcheye/protocol/frame_message.hpp"
#include "catcheye/roi/roi_validation.hpp"
#include "catcheye/utils/logger.hpp"
#include "guard/detector_factory.hpp" // create_detector
#include "guard/preview_renderer.hpp"
#include "guard/roi_reload.hpp"

namespace catcheye {
namespace {

using RoiEvaluationStatus = catcheye::roi::EvaluationStatus;
constexpr std::uint64_t ROI_RELOAD_INTERVAL_FRAMES = 15;

std::string roi_status_to_string(RoiEvaluationStatus status) {
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

std::string escape_json(std::string_view value) {
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

std::string build_metadata_json(const std::vector<EvaluatedDetection>& detections, const IDetector& detector, bool roi_enabled) {
    std::ostringstream oss;
    oss << "{\"roi_enabled\":" << (roi_enabled ? "true" : "false") << ",\"detection_count\":" << detections.size() << ",\"detections\":[";

    for (std::size_t i = 0; i < detections.size(); ++i) {
        const auto& item = detections[i];
        if (i > 0) {
            oss << ',';
        }
        oss << "{" << "\"class_id\":" << item.detection.class_id << ",\"class_name\":\""
            << escape_json(detector.class_name(item.detection.class_id)) << "\"" << ",\"score\":" << item.detection.score << ",\"bbox\":{"
            << "\"x\":" << item.detection.box.x << ",\"y\":" << item.detection.box.y << ",\"width\":" << item.detection.box.width
            << ",\"height\":" << item.detection.box.height << "}" << ",\"roi_status\":\"" << roi_status_to_string(item.roi_result.status)
            << "\"" << ",\"roi_reason\":\"" << escape_json(item.roi_result.reason) << "\"" << "}";
    }

    oss << "]}";
    return oss.str();
}

cv::Mat frame_to_bgr_mat(const catcheye::input::Frame& frame) {
    if (frame.empty() || frame.width <= 0 || frame.height <= 0 || frame.stride <= 0) {
        return {};
    }

    const std::size_t expected_size = catcheye::input::frame_data_size(frame.format, frame.stride, frame.height);
    if (frame.data.size() < expected_size) {
        return {};
    }

    auto* raw = const_cast<std::uint8_t*>(frame.data.data());
    switch (frame.format) {
    case catcheye::input::PixelFormat::BGR: {
        cv::Mat wrapped(frame.height, frame.width, CV_8UC3, raw, static_cast<std::size_t>(frame.stride));
        return wrapped.clone();
    }
    case catcheye::input::PixelFormat::RGB: {
        cv::Mat wrapped(frame.height, frame.width, CV_8UC3, raw, static_cast<std::size_t>(frame.stride));
        cv::Mat bgr;
        cv::cvtColor(wrapped, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }
    case catcheye::input::PixelFormat::RGBA: {
        cv::Mat wrapped(frame.height, frame.width, CV_8UC4, raw, static_cast<std::size_t>(frame.stride));
        cv::Mat bgr;
        cv::cvtColor(wrapped, bgr, cv::COLOR_RGBA2BGR);
        return bgr;
    }
    case catcheye::input::PixelFormat::BGRA: {
        cv::Mat wrapped(frame.height, frame.width, CV_8UC4, raw, static_cast<std::size_t>(frame.stride));
        cv::Mat bgr;
        cv::cvtColor(wrapped, bgr, cv::COLOR_BGRA2BGR);
        return bgr;
    }
    case catcheye::input::PixelFormat::GRAY8: {
        cv::Mat wrapped(frame.height, frame.width, CV_8UC1, raw, static_cast<std::size_t>(frame.stride));
        cv::Mat bgr;
        cv::cvtColor(wrapped, bgr, cv::COLOR_GRAY2BGR);
        return bgr;
    }
    case catcheye::input::PixelFormat::NV12: {
        cv::Mat wrapped(frame.height + (frame.height / 2), frame.width, CV_8UC1, raw, static_cast<std::size_t>(frame.stride));
        cv::Mat bgr;
        cv::cvtColor(wrapped, bgr, cv::COLOR_YUV2BGR_NV12);
        return bgr;
    }
    case catcheye::input::PixelFormat::UNKNOWN:
        break;
    }

    return {};
}

bool build_annotated_publish_frame(const catcheye::input::Frame& source_frame,
                                   const std::vector<EvaluatedDetection>& detections,
                                   const IDetector& detector,
                                   const GuardProcessorConfig& config,
                                   catcheye::input::Frame& output_frame) {
    cv::Mat bgr = frame_to_bgr_mat(source_frame);
    if (bgr.empty()) {
        return false;
    }

    if (config.roi_enabled) {
        draw_roi_zones(bgr, config.roi_config);
    }
    draw_detections(bgr, detections, detector, config.roi_enabled);

    output_frame.data.assign(bgr.datastart, bgr.dataend);
    output_frame.width = bgr.cols;
    output_frame.height = bgr.rows;
    output_frame.stride = static_cast<int>(bgr.step);
    output_frame.format = catcheye::input::PixelFormat::BGR;
    output_frame.timestamp = source_frame.timestamp;
    return true;
}

} // namespace

GuardProcessor::GuardProcessor(GuardProcessorConfig config)
    : config_(std::move(config)), detector_(create_detector(config_.detector))
{
    if (const auto log = logger()) {
        log->info("GuardProcessor created with 'ncnn' backend");
    }
}

bool GuardProcessor::initialize() {
    if (config_.roi_enabled) {
        const auto validation = catcheye::roi::validate_camera_roi_config(config_.roi_config);
        if (!validation.valid) {
            if (const auto log = logger()) {
                log->error("ROI config invalid: {} issue(s)", validation.issues.size());
                for (const auto& issue : validation.issues) {
                    log->error("ROI issue: zone={}, point={}, msg={}", issue.zone_index, issue.point_index, issue.message);
                }
            }
            return false;
        }
    }

    initialize_roi_reload_watch(config_, last_seen_roi_config_text_, roi_reload_watch_warning_emitted_);

    if (!detector_->initialize()) {
        if (const auto log = logger()) {
            log->error("failed to initialize detector");
        }
        return false;
    }

    return true;
}

catcheye::runtime::ProcessOutput GuardProcessor::process(const catcheye::input::Frame& frame,
                                                         const catcheye::runtime::ProcessContext& context) {
    if (context.frame_index % ROI_RELOAD_INTERVAL_FRAMES == 0U) {
        try_reload_roi_config(config_, last_seen_roi_config_text_, roi_reload_watch_warning_emitted_);
    }

    if (context.should_process) {
        cached_detections_ = detector_->detect(frame);
    }

    const std::vector<Detection> visible_detections =
        filter_detections(cached_detections_, config_.filter_by_class, config_.filter_class_id);
    const std::vector<EvaluatedDetection> evaluated_detections =
        evaluate_detections(visible_detections, config_.roi_enabled, config_.roi_config);

    if (config_.roi_enabled) {
        for (const EvaluatedDetection& ed : evaluated_detections) {
            if (ed.roi_result.status == RoiEvaluationStatus::Restricted) {
                if (const auto log = logger()) {
                    log->warn("ROI restricted: frame={}, class_id={}, score={:.2f}, reason={}", context.frame_index, ed.detection.class_id,
                              ed.detection.score, ed.roi_result.reason);
                }
            }
        }
    }

    catcheye::runtime::ProcessOutput output;
    if (!context.needs_publish) {
        return output;
    }

    output.has_message = true;
    output.message.stream_name = "person-guard";
    output.message.metadata_json = build_metadata_json(evaluated_detections, *detector_, config_.roi_enabled);

    if (build_annotated_publish_frame(frame, evaluated_detections, *detector_, config_, output.publish_frame)) {
        output.has_publish_frame = true;
    }
    return output;
}

} // namespace catcheye
