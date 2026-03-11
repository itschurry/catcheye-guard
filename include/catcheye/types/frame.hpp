#pragma once

#include <cstdint>
#include <memory>

#include "catcheye/types/pixel_format.hpp"
#include "catcheye/types/timestamp.hpp"

namespace catcheye {

struct FrameBuffer {
    std::unique_ptr<uint8_t[]> memory;
};

struct Frame {
    uint8_t* data = nullptr;
    size_t data_size = 0;

    int width = 0;
    int height = 0;
    int stride = 0;

    PixelFormat format = PixelFormat::UNKNOWN;
    Timestamp timestamp = 0;
};

} // namespace catcheye
