#pragma once

#include <string>
#include <vector>

#include "catcheye/guard/roi/roi_polygon.hpp"

namespace catcheye::guard::roi {

struct CameraRoiConfig {
    std::string camera_id;
    int image_width {0};
    int image_height {0};
    std::vector<RoiPolygon> allowed_zones;
};

} // namespace catcheye::guard::roi
