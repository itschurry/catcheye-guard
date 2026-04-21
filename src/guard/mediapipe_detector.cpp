#include "guard/mediapipe_detector.hpp"

#include <algorithm>
#include <utility>

#include <opencv2/imgproc.hpp>

// MediaPipe Tasks C++ API
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/tasks/cc/components/containers/detection_result.h"
#include "mediapipe/tasks/cc/vision/core/image.h"

#include "catcheye/utils/logger.hpp"

namespace catcheye {
namespace {

// absl::Status 에러를 로그로 출력하고 false 반환하는 헬퍼
bool log_and_fail(const std::string& context, const absl::Status& status)
{
    if (const auto log = logger()) {
        log->error("{}: {}", context, status.message());
    }
    return false;
}

} // namespace

MediaPipeDetector::MediaPipeDetector(MediaPipeDetectorConfig config)
    : config_(std::move(config)) {}

bool MediaPipeDetector::initialize()
{
    if (initialized_) {
        return true;
    }

    namespace mp_vision = mediapipe::tasks::vision;
    namespace mp_core   = mediapipe::tasks::vision::core;

    auto options = std::make_unique<mp_vision::ObjectDetectorOptions>();

    // ── 기본 설정 ──────────────────────────────────────────────
    options->base_options.model_asset_path = config_.model_path;
    options->base_options.delegate         = mediapipe::tasks::core::Delegate::CPU;

    // ── 검출 파라미터 ──────────────────────────────────────────
    options->score_threshold    = config_.score_threshold;
    options->max_results        = config_.max_results;
    options->category_allowlist = config_.category_allowlist;

    // ── 추론 모드: IMAGE (단일 프레임, 동기 추론) ──────────────
    options->running_mode = mp_vision::core::RunningMode::IMAGE;

    if (const auto log = logger()) {
        log->info(
            "initializing MediaPipeDetector: model='{}', threshold={:.2f}, threads={}",
            config_.model_path, config_.score_threshold, config_.num_threads);
    }

    // Create() 는 absl::StatusOr<unique_ptr<ObjectDetector>> 반환
    auto create_result = mp_vision::ObjectDetector::Create(std::move(options));
    if (!create_result.ok()) {
        return log_and_fail("MediaPipeDetector::initialize", create_result.status());
    }

    mp_detector_ = std::move(*create_result);
    initialized_ = true;

    if (const auto log = logger()) {
        log->info("MediaPipeDetector initialized successfully");
    }
    return true;
}

bool MediaPipeDetector::is_initialized() const
{
    return initialized_;
}

std::vector<Detection> MediaPipeDetector::detect(const catcheye::input::Frame& frame)
{
    if (!initialized_ || frame.empty()) {
        return {};
    }

    // ── BGR(OpenCV) → RGB(MediaPipe) 변환 ─────────────────────
    cv::Mat rgb;
    cv::cvtColor(frame.image, rgb, cv::COLOR_BGR2RGB);

    // ── cv::Mat → mediapipe::Image 래핑 ───────────────────────
    // ImageFrame 은 데이터를 복사하지 않고 참조 (rgb 의 수명 주의)
    auto image_frame = std::make_shared<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB,
        rgb.cols,
        rgb.rows,
        mediapipe::ImageFrame::kDefaultAlignmentBoundary);

    // cv::Mat 의 픽셀을 ImageFrame 버퍼에 복사
    cv::Mat image_frame_mat = mediapipe::formats::MatView(image_frame.get());
    rgb.copyTo(image_frame_mat);

    mediapipe::Image mp_image(std::move(image_frame));

    // ── 추론 실행 ──────────────────────────────────────────────
    auto detect_result = mp_detector_->Detect(mp_image);
    if (!detect_result.ok()) {
        if (const auto log = logger()) {
            log->error("MediaPipe detect failed: {}", detect_result.status().message());
        }
        return {};
    }

    const auto& mp_detections = detect_result->detections;

    // ── MediaPipe Detection → catcheye::Detection 변환 ────────
    std::vector<Detection> detections;
    detections.reserve(mp_detections.size());

    for (const auto& mp_det : mp_detections) {
        detections.push_back(convert_detection(mp_det, frame.width(), frame.height()));
    }

    if (const auto log = logger()) {
        log->debug("MediaPipe detected {} objects", detections.size());
    }

    return detections;
}

Detection MediaPipeDetector::convert_detection(
    const mediapipe::tasks::components::containers::Detection& mp_det,
    int frame_width,
    int frame_height)
{
    Detection det;

    // ── 클래스 정보 ────────────────────────────────────────────
    if (!mp_det.categories.empty()) {
        const auto& category = mp_det.categories.front();
        det.class_id = category.index.value_or(0);
        det.score    = category.score;

        // 클래스명 캐시 업데이트
        if (category.category_name.has_value() && !category.category_name->empty()) {
            class_name_cache_[det.class_id] = *category.category_name;
        }
    }

    // ── Bounding Box ───────────────────────────────────────────
    // MediaPipe Tasks 는 픽셀 좌표 절대값으로 반환
    if (mp_det.bounding_box.has_value()) {
        const auto& bb = *mp_det.bounding_box;

        // 프레임 경계 내로 클리핑
        const float x1 = std::clamp(bb.left,   0.0F, static_cast<float>(frame_width  - 1));
        const float y1 = std::clamp(bb.top,    0.0F, static_cast<float>(frame_height - 1));
        const float x2 = std::clamp(bb.right,  0.0F, static_cast<float>(frame_width  - 1));
        const float y2 = std::clamp(bb.bottom, 0.0F, static_cast<float>(frame_height - 1));

        det.box.x      = x1;
        det.box.y      = y1;
        det.box.width  = std::max(0.0F, x2 - x1);
        det.box.height = std::max(0.0F, y2 - y1);
    }

    return det;
}

std::string MediaPipeDetector::class_name(int class_id) const
{
    const auto it = class_name_cache_.find(class_id);
    return (it != class_name_cache_.end()) ? it->second : "cls:" + std::to_string(class_id);
}

} // namespace catcheye
