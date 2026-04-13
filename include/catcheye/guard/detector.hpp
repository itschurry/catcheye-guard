#pragma once

#include <map>
#include <string>
#include <vector>

#include <net.h>

#include "catcheye/types/bounding_box.hpp"
#include "catcheye/input/frame.hpp"

namespace catcheye {

struct Detection {
    int class_id = -1;
    float score = 0.0F;
    BoundingBox box{};
};

struct DetectorConfig {
    std::string param_path;
    std::string bin_path;
    std::string metadata_path;
    std::string input_blob_name = "in0";
    std::string output_blob_name = "out0";
    int input_width = 640;
    int input_height = 640;
    float confidence_threshold = 0.25F;
    float nms_threshold = 0.45F;
    int num_threads = 2;
    bool use_vulkan_compute = false;
};

class Detector {
   public:
    explicit Detector(DetectorConfig config = {});

    bool initialize();
    bool is_initialized() const;
    std::vector<Detection> detect(const catcheye::input::Frame& frame);
    std::string class_name(int class_id) const;

   private:
    DetectorConfig config_;
    ncnn::Net net_;
    std::map<int, std::string> class_names_;
    bool initialized_ = false;
};

} // namespace catcheye
