#pragma once

#include <string>
#include <vector>

#include "catcheye/guard/roi/point.hpp"

namespace catcheye::guard::roi {

struct RoiPolygon {
    std::string id;
    std::string name;
    bool enabled {true};
    std::vector<Point> points;
};

} // namespace catcheye::guard::roi
