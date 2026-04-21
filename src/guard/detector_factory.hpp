#pragma once

#include <memory>

#include "guard/detector_interface.hpp"
#include "guard/ncnn_detector.hpp"

namespace catcheye {

struct DetectorFactoryConfig {
    NcnnDetectorConfig ncnn;
};

inline std::unique_ptr<IDetector> create_detector(const DetectorFactoryConfig& cfg)
{
    return std::make_unique<NcnnDetector>(cfg.ncnn);
}

} // namespace catcheye
