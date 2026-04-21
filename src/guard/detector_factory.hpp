#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "guard/detector_interface.hpp"
#include "guard/mediapipe_detector.hpp"
#include "guard/ncnn_detector.hpp"

namespace catcheye {

// processor_config.hpp 에 정의된 enum 과 맞춤
enum class DetectorBackend {
    Ncnn,       // 기존 NCNN + YOLO (기본값)
    MediaPipe,  // MediaPipe Tasks + EfficientDet-Lite
};

struct DetectorFactoryConfig {
    DetectorBackend backend = DetectorBackend::Ncnn;

    // NCNN 전용
    NcnnDetectorConfig ncnn;

    // MediaPipe 전용
    MediaPipeDetectorConfig mediapipe;
};

inline std::unique_ptr<IDetector> create_detector(const DetectorFactoryConfig& cfg)
{
    switch (cfg.backend) {
        case DetectorBackend::Ncnn:
            return std::make_unique<NcnnDetector>(cfg.ncnn);

        case DetectorBackend::MediaPipe:
            return std::make_unique<MediaPipeDetector>(cfg.mediapipe);
    }

    throw std::runtime_error("unknown DetectorBackend");
}

// 백엔드 이름을 문자열로 반환 (로그용)
inline std::string backend_name(DetectorBackend backend)
{
    switch (backend) {
        case DetectorBackend::Ncnn:       return "ncnn";
        case DetectorBackend::MediaPipe:  return "mediapipe";
    }
    return "unknown";
}

} // namespace catcheye
