#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "guard/detector_interface.hpp"

// MediaPipe Tasks C++ — Object Detector
// 빌드 시 CMakeLists.txt 에 mediapipe_tasks 링크 필요 (하단 참고)
#include "mediapipe/tasks/cc/vision/object_detector/object_detector.h"

namespace catcheye {

struct MediaPipeDetectorConfig {
    // EfficientDet-Lite TFLite 모델 경로
    // 다운로드: https://storage.googleapis.com/mediapipe-models/
    //   object_detector/efficientdet_lite0/int8/1/efficientdet_lite0.tflite
    std::string model_path;

    float score_threshold = 0.35F;
    int   max_results     = 10;

    // 빈 벡터 = 전체 클래스 검출. {"person"} 으로 지정하면 사람만 필터링
    std::vector<std::string> category_allowlist = {"person"};

    int num_threads = 2;  // CPU 추론 스레드 수 (RPi5: 최대 4)
};

class MediaPipeDetector final : public IDetector {
public:
    explicit MediaPipeDetector(MediaPipeDetectorConfig config = {});

    bool initialize() override;
    bool is_initialized() const override;
    std::vector<Detection> detect(const catcheye::input::Frame& frame) override;

    // MediaPipe는 모델 자체에 클래스명이 포함되어 있으므로
    // class_id 대신 내부 매핑 테이블을 사용
    std::string class_name(int class_id) const override;

private:
    MediaPipeDetectorConfig config_;
    std::unique_ptr<mediapipe::tasks::vision::ObjectDetector> mp_detector_;
    std::map<int, std::string> class_name_cache_;
    bool initialized_ = false;

    // MediaPipe Detection → catcheye::Detection 변환
    Detection convert_detection(
        const mediapipe::tasks::components::containers::Detection& mp_det,
        int frame_width,
        int frame_height);
};

} // namespace catcheye
