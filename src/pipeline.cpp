#include "catcheye/core/pipeline.hpp"

#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace catcheye {
namespace {

cv::Rect to_rect(const BoundingBox& box) {
    return cv::Rect(
        static_cast<int>(box.x),
        static_cast<int>(box.y),
        static_cast<int>(box.width),
        static_cast<int>(box.height));
}

std::vector<Detection> filter_detections(
    const std::vector<Detection>& detections,
    bool enabled,
    int class_id) {
    if (!enabled) {
        return detections;
    }

    std::vector<Detection> filtered;
    filtered.reserve(detections.size());
    for (const Detection& detection : detections) {
        if (detection.class_id == class_id) {
            filtered.push_back(detection);
        }
    }

    return filtered;
}

void draw_detections(
    cv::Mat& image,
    const std::vector<Detection>& detections,
    const Detector& detector) {
    for (const Detection& detection : detections) {
        const cv::Rect box = to_rect(detection.box);
        cv::rectangle(image, box, cv::Scalar(0, 255, 0), 2);

        const std::string label =
            detector.class_name(detection.class_id) + " " + cv::format("%.2f", detection.score);

        int baseline = 0;
        const cv::Size text_size = cv::getTextSize(
            label,
            cv::FONT_HERSHEY_SIMPLEX,
            0.5,
            1,
            &baseline);

        const int label_x = std::max(box.x, 0);
        const int label_y = std::max(box.y - 4, text_size.height + 4);
        const cv::Rect background(
            label_x,
            label_y - text_size.height - 6,
            text_size.width + 6,
            text_size.height + 6);

        cv::rectangle(image, background, cv::Scalar(0, 255, 0), cv::FILLED);
        cv::putText(
            image,
            label,
            cv::Point(label_x + 3, label_y - 3),
            cv::FONT_HERSHEY_SIMPLEX,
            0.5,
            cv::Scalar(0, 0, 0),
            1);
    }
}

} // namespace

Pipeline::Pipeline(PipelineConfig config)
    : config_(std::move(config)),
      camera_(config_.camera),
      detector_(config_.detector) {}

int Pipeline::run() {
    if (!camera_.open()) {
        std::cerr << "Failed to open camera pipeline.\n";
        return 1;
    }

    if (!detector_.initialize()) {
        std::cerr << "Failed to initialize detector.\n";
        camera_.close();
        return 1;
    }

    Frame frame;
    while (true) {
        if (!camera_.read(frame)) {
            std::cerr << "Failed to read frame.\n";
            break;
        }

        const std::vector<Detection> detections = detector_.detect(frame);
        const std::vector<Detection> visible_detections = filter_detections(
            detections,
            config_.filter_by_class,
            config_.filter_class_id);

        if (config_.render_preview) {
            cv::Mat preview = frame.image.clone();
            draw_detections(preview, visible_detections, detector_);

            cv::imshow(config_.window_name, preview);
            const int key = cv::waitKey(1);
            if (key == 27 || key == 'q') {
                break;
            }
        }
    }

    camera_.close();
    cv::destroyAllWindows();
    return 0;
}

} // namespace catcheye
