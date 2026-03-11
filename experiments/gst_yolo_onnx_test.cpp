#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct Detection {
    int class_id;
    float score;
    cv::Rect box;
};

static cv::Mat letterbox(
    const cv::Mat& image,
    int new_width,
    int new_height,
    float& scale,
    int& pad_w,
    int& pad_h)
{
    int orig_w = image.cols;
    int orig_h = image.rows;

    scale = std::min(new_width / (float)orig_w, new_height / (float)orig_h);
    int resized_w = static_cast<int>(std::round(orig_w * scale));
    int resized_h = static_cast<int>(std::round(orig_h * scale));

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(resized_w, resized_h));

    pad_w = new_width - resized_w;
    pad_h = new_height - resized_h;

    int top = pad_h / 2;
    int bottom = pad_h - top;
    int left = pad_w / 2;
    int right = pad_w - left;

    cv::Mat output;
    cv::copyMakeBorder(
        resized, output,
        top, bottom, left, right,
        cv::BORDER_CONSTANT,
        cv::Scalar(114, 114, 114));

    return output;
}

static std::vector<Detection> decodeYOLO(
    const cv::Mat& output,
    float conf_threshold,
    float nms_threshold,
    float scale,
    int pad_w,
    int pad_h,
    int orig_w,
    int orig_h)
{
    std::vector<int> class_ids;
    std::vector<float> scores;
    std::vector<cv::Rect> boxes;

    // Ultralytics 계열 ONNX는 흔히 [1, C, N] 또는 [1, N, C] 형태
    cv::Mat out = output;

    int dims = out.dims;
    if (dims != 3) {
        std::cerr << "Unexpected output dims: " << dims << std::endl;
        return {};
    }

    int d0 = out.size[0];
    int d1 = out.size[1];
    int d2 = out.size[2];

    cv::Mat dets;

    // [1, C, N] -> [N, C]
    if (d0 == 1 && d1 < d2) {
        cv::Mat reshaped(d1, d2, CV_32F, (void*)out.ptr<float>());
	cv::Mat tmp = reshaped.t();
	dets = tmp.clone();
    }
    // [1, N, C] -> [N, C]
    else if (d0 == 1 && d1 > d2) {
        dets = cv::Mat(d1, d2, CV_32F, (void*)out.ptr<float>()).clone();
    } else {
        std::cerr << "Unsupported output shape: "
                  << d0 << " x " << d1 << " x " << d2 << std::endl;
        return {};
    }

    const int num_rows = dets.rows;
    const int num_cols = dets.cols;

    if (num_cols < 6) {
        std::cerr << "Unexpected detection width: " << num_cols << std::endl;
        return {};
    }

    for (int i = 0; i < num_rows; ++i) {
        const float* data = dets.ptr<float>(i);

        float cx = data[0];
        float cy = data[1];
        float w  = data[2];
        float h  = data[3];

        // data[4:] = class logits/conf들로 가정
        int best_class = -1;
        float best_score = 0.0f;

        for (int c = 4; c < num_cols; ++c) {
            float score = data[c];
            if (score > best_score) {
                best_score = score;
                best_class = c - 4;
            }
        }

        if (best_score < conf_threshold) continue;

        float x1 = cx - w * 0.5f;
        float y1 = cy - h * 0.5f;
        float x2 = cx + w * 0.5f;
        float y2 = cy + h * 0.5f;

        // letterbox 역변환
        int left = pad_w / 2;
        int top  = pad_h / 2;

        x1 -= left;
        x2 -= left;
        y1 -= top;
        y2 -= top;

        x1 /= scale;
        x2 /= scale;
        y1 /= scale;
        y2 /= scale;

        x1 = std::max(0.0f, std::min(x1, (float)(orig_w - 1)));
        y1 = std::max(0.0f, std::min(y1, (float)(orig_h - 1)));
        x2 = std::max(0.0f, std::min(x2, (float)(orig_w - 1)));
        y2 = std::max(0.0f, std::min(y2, (float)(orig_h - 1)));

        int box_w = std::max(0, (int)(x2 - x1));
        int box_h = std::max(0, (int)(y2 - y1));

        if (box_w <= 1 || box_h <= 1) continue;

        class_ids.push_back(best_class);
        scores.push_back(best_score);
        boxes.emplace_back((int)x1, (int)y1, box_w, box_h);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, conf_threshold, nms_threshold, indices);

    std::vector<Detection> results;
    results.reserve(indices.size());

    for (int idx : indices) {
        results.push_back({class_ids[idx], scores[idx], boxes[idx]});
    }

    return results;
}

int main() {
    const std::string pipeline =
        "libcamerasrc ! "
        "video/x-raw,width=1280,height=720,framerate=30/1,format=NV12 ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink drop=true max-buffers=1 sync=false";

    const std::string model_path = "yolo26n.onnx";
    const int input_size = 640;
    const float conf_threshold = 0.25f;
    const float nms_threshold  = 0.45f;

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera pipeline.\n";
        return 1;
    }

    cv::dnn::Net net = cv::dnn::readNetFromONNX(model_path);
    if (net.empty()) {
        std::cerr << "Failed to load model: " << model_path << "\n";
        return 1;
    }

    // 라즈베리파이 CPU면 일단 이걸로
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    cv::Mat frame;
    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Failed to read frame.\n";
            break;
        }

        float scale = 1.0f;
        int pad_w = 0, pad_h = 0;
        cv::Mat input_image = letterbox(frame, input_size, input_size, scale, pad_w, pad_h);

        cv::Mat blob = cv::dnn::blobFromImage(
            input_image,
            1.0 / 255.0,
            cv::Size(input_size, input_size),
            cv::Scalar(),
            true,   // swapRB
            false   // crop
        );

        net.setInput(blob);
        cv::Mat output = net.forward();

        std::vector<Detection> detections = decodeYOLO(
            output,
            conf_threshold,
            nms_threshold,
            scale,
            pad_w,
            pad_h,
            frame.cols,
            frame.rows
        );

        for (const auto& det : detections) {
            cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);

            std::string label =
                "cls:" + std::to_string(det.class_id) +
                " " + cv::format("%.2f", det.score);

            int baseline = 0;
            cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

            int x = det.box.x;
            int y = std::max(det.box.y - 5, ts.height + 4);

            cv::rectangle(
                frame,
                cv::Rect(x, y - ts.height - 4, ts.width + 6, ts.height + 6),
                cv::Scalar(0, 255, 0),
                cv::FILLED
            );

            cv::putText(frame, label, cv::Point(x + 3, y - 3),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
        }

        cv::imshow("YOLO ONNX Test", frame);

        int key = cv::waitKey(1);
        if (key == 27 || key == 'q') break;
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
