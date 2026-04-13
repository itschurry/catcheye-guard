#pragma once

#include <chrono>

#include "catcheye/input/timestamp.hpp"

namespace catcheye {

inline catcheye::input::Timestamp now_millis() {
    return static_cast<catcheye::input::Timestamp>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace catcheye
