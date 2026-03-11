#pragma once

#include <string>

#include <opencv2/videoio.hpp>

#include "catcheye/types/frame.hpp"

namespace catcheye {

struct CameraConfig {
    std::string pipeline;
    int api_preference = cv::CAP_GSTREAMER;
};

class Camera {
   public:
    explicit Camera(CameraConfig config = {});

    bool open();
    bool is_open() const;
    bool read(Frame& frame);
    void close();

   private:
    CameraConfig config_;
    cv::VideoCapture capture_;
};

} // namespace catcheye
