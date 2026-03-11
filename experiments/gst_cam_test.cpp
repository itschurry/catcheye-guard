#include <opencv2/opencv.hpp>

#include <iostream>
#include <string>

int main() {
    // 캣치아이 스타일 GStreamer 파이프라인
    std::string pipeline =
        "libcamerasrc ! "
        "video/x-raw,width=1280,height=720,framerate=30/1,format=NV12 ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink drop=true max-buffers=1 sync=false";

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);

    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera pipeline.\n";
        return 1;
    }

    std::cout << "Camera pipeline opened successfully.\n";

    cv::Mat frame;
    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Failed to read frame.\n";
            break;
        }

        cv::imshow("GStreamer Camera Test", frame);

        int key = cv::waitKey(1);
        if (key == 27 || key == 'q') {  // ESC or q
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
