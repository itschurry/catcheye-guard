#pragma once

#include <string>
#include <vector>

#include "catcheye/input/frame.hpp"
#include "guard/bounding_box.hpp"

namespace catcheye {

struct Detection {
    int class_id = -1;
    float score = 0.0F;
    BoundingBox box{};
};

// 백엔드에 무관한 공통 인터페이스
class IDetector {
public:
    virtual ~IDetector() = default;

    virtual bool initialize() = 0;
    virtual bool is_initialized() const = 0;
    virtual std::vector<Detection> detect(const catcheye::input::Frame& frame) = 0;
    virtual std::string class_name(int class_id) const = 0;
};

} // namespace catcheye
