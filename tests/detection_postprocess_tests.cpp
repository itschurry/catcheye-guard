#include "guard/detection_postprocess.hpp"

#include <cassert>
#include <vector>

namespace {

catcheye::roi::CameraRoiConfig test_roi_config() {
    catcheye::roi::CameraRoiConfig config;
    config.camera_id = "test";
    config.image_width = 100;
    config.image_height = 100;
    config.allowed_zones.push_back(catcheye::roi::RoiPolygon{
        .id = "pallet",
        .name = "pallet",
        .enabled = true,
        .points = {
            {10.0, 10.0},
            {60.0, 10.0},
            {60.0, 60.0},
            {10.0, 60.0},
        },
    });
    return config;
}

catcheye::Detection detection(int class_id, float x, float y, float width, float height) {
    return catcheye::Detection{
        .class_id = class_id,
        .score = 0.9F,
        .box = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        },
    };
}

void test_class_filters() {
    const std::vector<catcheye::Detection> detections{
        detection(0, 10.0F, 10.0F, 10.0F, 10.0F),
        detection(1, 20.0F, 20.0F, 10.0F, 10.0F),
        detection(3, 30.0F, 30.0F, 10.0F, 10.0F),
    };

    const auto people = catcheye::filter_detections(detections, true, 0);
    const auto pallets = catcheye::filter_detections(detections, true, 1);

    assert(people.size() == 1);
    assert(people.front().class_id == 0);
    assert(pallets.size() == 1);
    assert(pallets.front().class_id == 1);
}

void test_pallet_fully_inside_is_present() {
    const auto config = test_roi_config();
    const std::vector<catcheye::Detection> detections{
        detection(1, 20.0F, 20.0F, 20.0F, 20.0F),
    };

    const auto pallets = catcheye::evaluate_pallet_detections(detections, true, config);
    assert(pallets.size() == 1);
    assert(pallets.front().present);
}

void test_pallet_partial_overlap_is_not_present() {
    const auto config = test_roi_config();
    const std::vector<catcheye::Detection> detections{
        detection(1, 50.0F, 50.0F, 20.0F, 20.0F),
    };

    const auto pallets = catcheye::evaluate_pallet_detections(detections, true, config);
    assert(pallets.size() == 1);
    assert(!pallets.front().present);
}

void test_missing_pallet_is_not_present() {
    const auto config = test_roi_config();
    const std::vector<catcheye::Detection> detections;

    const auto pallets = catcheye::evaluate_pallet_detections(detections, true, config);
    assert(pallets.empty());
}

} // namespace

int main() {
    test_class_filters();
    test_pallet_fully_inside_is_present();
    test_pallet_partial_overlap_is_not_present();
    test_missing_pallet_is_not_present();
    return 0;
}
