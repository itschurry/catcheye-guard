#include "catcheye/core/camera.hpp"

#include <utility>

#include "catcheye/types/pixel_format.hpp"
#include "catcheye/utils/time_utils.hpp"

namespace catcheye {

Camera::Camera(CameraConfig config)
    : config_(std::move(config)) {}

bool Camera::open() {
    if (capture_.isOpened()) {
        return true;
    }

    return capture_.open(config_.pipeline, config_.api_preference);
}

bool Camera::is_open() const {
    return capture_.isOpened();
}

bool Camera::read(Frame& frame) {
    cv::Mat image;
    if (!capture_.read(image) || image.empty()) {
        return false;
    }

    frame.image = std::move(image);
    frame.format = PixelFormat::BGR;
    frame.timestamp = now_millis();
    return true;
}

void Camera::close() {
    if (capture_.isOpened()) {
        capture_.release();
    }
}

} // namespace catcheye
