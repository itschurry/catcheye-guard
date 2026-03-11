#include "catcheye/core/camera.hpp"

#include <utility>

#include "catcheye/types/pixel_format.hpp"
#include "catcheye/utils/logger.hpp"
#include "catcheye/utils/time_utils.hpp"

namespace catcheye {

Camera::Camera(CameraConfig config)
    : config_(std::move(config)) {}

bool Camera::open() {
    if (capture_.isOpened()) {
        if (const auto log = logger()) {
            log->debug("camera already open");
        }
        return true;
    }

    const bool opened = capture_.open(config_.pipeline, config_.api_preference);
    if (const auto log = logger()) {
        if (opened) {
            log->info("camera opened with api {} and pipeline '{}'", config_.api_preference, config_.pipeline);
        } else {
            log->error("failed to open camera with api {} and pipeline '{}'", config_.api_preference, config_.pipeline);
        }
    }

    return opened;
}

bool Camera::is_open() const {
    return capture_.isOpened();
}

bool Camera::read(Frame& frame) {
    cv::Mat image;
    if (!capture_.read(image) || image.empty()) {
        if (const auto log = logger()) {
            log->warn("failed to read frame from camera");
        }
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
        if (const auto log = logger()) {
            log->info("camera closed");
        }
    }
}

} // namespace catcheye
