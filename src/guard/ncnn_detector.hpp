#pragma once

#include <map>
#include <string>
#include <vector>

#include <net.h>  // ncnn

#include "guard/detector_interface.hpp"

namespace catcheye {

struct NcnnDetectorConfig {
    std::string param_path;
    std::string bin_path;
    std::string metadata_path;
    std::string input_blob_name  = "in0";
    std::string output_blob_name = "out0";
    int input_width              = 640;
    int input_height             = 640;
    float confidence_threshold   = 0.25F;
    float nms_threshold          = 0.45F;
    int num_threads              = 2;
    bool use_vulkan_compute      = false;
};

class NcnnDetector final : public IDetector {
public:
    explicit NcnnDetector(NcnnDetectorConfig config = {});

    bool initialize() override;
    bool is_initialized() const override;
    std::vector<Detection> detect(const catcheye::input::Frame& frame) override;
    std::string class_name(int class_id) const override;

private:
    NcnnDetectorConfig config_;
    ncnn::Net net_;
    std::map<int, std::string> class_names_;
    bool initialized_ = false;
};

// ── 하위 호환: 기존 코드가 DetectorConfig / Detector 를 참조하는 경우 ──
using DetectorConfig = NcnnDetectorConfig;
using Detector       = NcnnDetector;

} // namespace catcheye
