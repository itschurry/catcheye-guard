#include "catcheye/core/frame_source.hpp"

#include <utility>

#include <opencv2/imgcodecs.hpp>

#include "catcheye/types/pixel_format.hpp"
#include "catcheye/utils/logger.hpp"
#include "catcheye/utils/time_utils.hpp"

namespace catcheye {
namespace {

std::string source_type_name(InputSourceType type) {
    switch (type) {
        case InputSourceType::Camera:
            return "camera";
        case InputSourceType::VideoFile:
            return "video";
        case InputSourceType::ImageFile:
            return "image";
    }

    return "unknown";
}

std::string capture_target(const InputSourceConfig& config) {
    if (config.type == InputSourceType::Camera) {
        if (!config.camera_pipeline.empty()) {
            return config.camera_pipeline;
        }
        return default_camera_pipeline();
    }

    return config.uri;
}

int capture_api_preference(const InputSourceConfig& config) {
    if (config.type == InputSourceType::Camera && config.api_preference == cv::CAP_ANY) {
        return cv::CAP_GSTREAMER;
    }
    return config.api_preference;
}

} // namespace

OpenCvCaptureSource::OpenCvCaptureSource(InputSourceConfig config)
    : config_(std::move(config)) {}

bool OpenCvCaptureSource::open() {
    if (capture_.isOpened()) {
        if (const auto log = logger()) {
            log->debug("input source already open: {}", describe());
        }
        return true;
    }

    const std::string target = capture_target(config_);
    const int api = capture_api_preference(config_);
    const bool opened = capture_.open(target, api);
    if (const auto log = logger()) {
        if (opened) {
            log->info("opened input source '{}' with api {}", describe(), api);
        } else {
            log->error("failed to open input source '{}' with api {}", describe(), api);
        }
    }
    return opened;
}

bool OpenCvCaptureSource::is_open() const {
    return capture_.isOpened();
}

FrameReadStatus OpenCvCaptureSource::read(Frame& frame) {
    cv::Mat image;
    if (!capture_.read(image)) {
        if (const auto log = logger()) {
            log->info("input source reached end or failed to read: {}", describe());
        }
        return capture_.isOpened() ? FrameReadStatus::EndOfStream : FrameReadStatus::Error;
    }
    if (image.empty()) {
        if (const auto log = logger()) {
            log->warn("input source returned empty frame: {}", describe());
        }
        return FrameReadStatus::Error;
    }

    frame.image = std::move(image);
    frame.format = PixelFormat::BGR;
    frame.timestamp = now_millis();
    return FrameReadStatus::Ok;
}

void OpenCvCaptureSource::close() {
    if (capture_.isOpened()) {
        capture_.release();
        if (const auto log = logger()) {
            log->info("closed input source '{}'", describe());
        }
    }
}

std::string OpenCvCaptureSource::describe() const {
    return source_type_name(config_.type) + ":" + capture_target(config_);
}

ImageFileSource::ImageFileSource(InputSourceConfig config)
    : config_(std::move(config)) {}

bool ImageFileSource::open() {
    if (opened_) {
        if (const auto log = logger()) {
            log->debug("input source already open: {}", describe());
        }
        return true;
    }

    image_ = cv::imread(config_.uri, cv::IMREAD_COLOR);
    opened_ = !image_.empty();
    delivered_ = false;
    if (const auto log = logger()) {
        if (opened_) {
            log->info("opened input source '{}'", describe());
        } else {
            log->error("failed to open input source '{}'", describe());
        }
    }
    return opened_;
}

bool ImageFileSource::is_open() const {
    return opened_;
}

FrameReadStatus ImageFileSource::read(Frame& frame) {
    if (!opened_) {
        return FrameReadStatus::Error;
    }
    if (delivered_) {
        return FrameReadStatus::EndOfStream;
    }
    if (image_.empty()) {
        return FrameReadStatus::Error;
    }

    frame.image = image_.clone();
    frame.format = PixelFormat::BGR;
    frame.timestamp = now_millis();
    delivered_ = true;
    return FrameReadStatus::Ok;
}

void ImageFileSource::close() {
    image_.release();
    opened_ = false;
    delivered_ = false;
    if (const auto log = logger()) {
        log->info("closed input source '{}'", describe());
    }
}

std::string ImageFileSource::describe() const {
    return source_type_name(config_.type) + ":" + config_.uri;
}

std::unique_ptr<FrameSource> create_frame_source(InputSourceConfig config) {
    switch (config.type) {
        case InputSourceType::Camera:
        case InputSourceType::VideoFile:
            return std::make_unique<OpenCvCaptureSource>(std::move(config));
        case InputSourceType::ImageFile:
            return std::make_unique<ImageFileSource>(std::move(config));
    }

    return nullptr;
}

std::string default_camera_pipeline() {
    return "libcamerasrc ! "
           "video/x-raw,width=1280,height=720,framerate=15/1,format=NV12 ! "
           "videoflip video-direction=vert ! "
           "videoconvert ! "
           "video/x-raw,format=BGR ! "
           "appsink drop=true max-buffers=1 sync=false";
}

} // namespace catcheye
