#pragma once

#include <chrono>

#include "catcheye/types/timestamp.hpp"

namespace catcheye {

inline Timestamp now_millis() {
    return static_cast<Timestamp>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace catcheye
