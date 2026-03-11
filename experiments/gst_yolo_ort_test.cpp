#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
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

    scale = std::min(new_width / static_cast<float>(orig_w),
                     new_height / static_cast<float>(orig_h));

    int resized_w = static_cast<int>(std::round(orig_w * scale));
    int resized_h = static_cast<int>(std::round(orig_h * scale));

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(resized_w, resized_h));

    pad_w = new_width - resized_w;
    pad_h = new_height - resized_h;

    int left = pad_w / 2;
    int right = pad_w - left;
    int top = pad_h / 2;
    int bottom = pad_h - top;

    cv::Mat output;
    cv::copyMakeBorder(
        resized, output,
        top, bottom, left, right,
        cv::BORDER_CONSTANT,
        cv::Scalar(114, 114, 114));

    return output;
}

static std::vector<Detection> decode_yolo_output(
    const float* output_data,
    const std::vector<int64_t>& output_shape,
    float conf_threshold,
    float nms_threshold,
    float scale,
    int pad_w,
    int pad_h,
    int orig_w,
    int orig_h)
{
    std::vector<Detection> results;
    std::vector<int> class_ids;
    std::vector<float> scores;
    std::vector<cv::Rect> boxes;

    if (output_shape.size() != 3) {
        std::cerr << "Unexpected output rank: " << output_shape.size() << std::endl;
        return results;
    }

    const int64_t b = output_shape[0];
    const int64_t a = output_shape[1];
    const int64_t c = output_shape[2];

    if (b != 1) {
        std::cerr << "Unexpected batch size: " << b << std::endl;
        return results;
    }

    // 흔한 형태:
    // [1, 84, 8400]  -> channels-first
    // [1, 8400, 84]  -> rows-first
    int num_rows = 0;
    int num_cols = 0;
    bool transposed = false;

    if (a < c) {
        // [1, C, N]
        num_cols = static_cast<int>(a);
        num_rows = static_cast<int>(c);
        transposed = true;
    } else {
        // [1, N, C]
        num_rows = static_cast<int>(a);
        num_cols = static_cast<int>(c);
        transposed = false;
    }

    if (num_cols < 6) {
        std::cerr << "Unexpected output width: " << num_cols << std::endl;
        return results;
    }

    const int left = pad_w / 2;
    const int top = pad_h / 2;

    for (int i = 0; i < num_rows; ++i) {
        float cx, cy, w, h;

        if (transposed) {
            cx = output_data[0 * num_rows + i];
            cy = output_data[1 * num_rows + i];
            w  = output_data[2 * num_rows + i];
            h  = output_data[3 * num_rows + i];
        } else {
            const float* row = output_data + i * num_cols;
            cx = row[0];
            cy = row[1];
            w  = row[2];
            h  = row[3];
        }

        int best_class = -1;
        float best_score = 0.0f;

        for (int cls = 4; cls < num_cols; ++cls) {
            float score = transposed
                ? output_data[cls * num_rows + i]
                : output_data[i * num_cols + cls];

            if (score > best_score) {
                best_score = score;
                best_class = cls - 4;
            }
        }

        if (best_score < conf_threshold) continue;

        float x1 = cx - 0.5f * w;
        float y1 = cy - 0.5f * h;
        float x2 = cx + 0.5f * w;
        float y2 = cy + 0.5f * h;

        // letterbox 역변환
        x1 -= left;
        x2 -= left;
        y1 -= top;
        y2 -= top;

        x1 /= scale;
        x2 /= scale;
        y1 /= scale;
        y2 /= scale;

        x1 = std::max(0.0f, std::min(x1, static_cast<float>(orig_w - 1)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(orig_h - 1)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(orig_w - 1)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(orig_h - 1)));

        int bw = std::max(0, static_cast<int>(x2 - x1));
        int bh = std::max(0, static_cast<int>(y2 - y1));
        if (bw <= 1 || bh <= 1) continue;

        class_ids.push_back(best_class);
        scores.push_back(best_score);
        boxes.emplace_back(static_cast<int>(x1), static_cast<int>(y1), bw, bh);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, conf_threshold, nms_threshold, indices);

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
    const float nms_threshold = 0.45f;

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera pipeline." << std::endl;
        return 1;
    }

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "yolo26");
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(2);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    Ort::Session session(env, model_path.c_str(), session_options);
    Ort::AllocatorWithDefaultOptions allocator;

    auto input_name_alloc = session.GetInputNameAllocated(0, allocator);
    auto output_name_alloc = session.GetOutputNameAllocated(0, allocator);

    const char* input_name = input_name_alloc.get();
    const char* output_name = output_name_alloc.get();

    std::cout << "Input  name: " << input_name << std::endl;
    std::cout << "Output name: " << output_name << std::endl;

    cv::Mat frame;
    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Failed to read frame." << std::endl;
            break;
        }

        float scale = 1.0f;
        int pad_w = 0;
        int pad_h = 0;
        cv::Mat input_img = letterbox(frame, input_size, input_size, scale, pad_w, pad_h);

        cv::Mat rgb;
        cv::cvtColor(input_img, rgb, cv::COLOR_BGR2RGB);

        rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

        std::vector<float> input_tensor_values(1 * 3 * input_size * input_size);
        const int channel_size = input_size * input_size;

        for (int y = 0; y < input_size; ++y) {
            for (int x = 0; x < input_size; ++x) {
                const cv::Vec3f& pixel = rgb.at<cv::Vec3f>(y, x);
                input_tensor_values[0 * channel_size + y * input_size + x] = pixel[0];
                input_tensor_values[1 * channel_size + y * input_size + x] = pixel[1];
                input_tensor_values[2 * channel_size + y * input_size + x] = pixel[2];
            }
        }

        std::array<int64_t, 4> input_shape{1, 3, input_size, input_size};

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input_tensor_values.data(),
            input_tensor_values.size(),
            input_shape.data(),
            input_shape.size());

        const char* input_names[] = {input_name};
        const char* output_names[] = {output_name};

        auto output_tensors = session.Run(
            Ort::RunOptions{nullptr},
            input_names, &input_tensor, 1,
            output_names, 1);

        if (output_tensors.empty() || !output_tensors[0].IsTensor()) {
            std::cerr << "Invalid output tensor." << std::endl;
            break;
        }

        auto output_info = output_tensors[0].GetTensorTypeAndShapeInfo();
        std::vector<int64_t> output_shape = output_info.GetShape();
        const float* output_data = output_tensors[0].GetTensorData<float>();

        static bool printed_shape = false;
        if (!printed_shape) {
            std::cout << "Output shape: ";
            for (auto s : output_shape) std::cout << s << " ";
            std::cout << std::endl;
            printed_shape = true;
        }

        auto detections = decode_yolo_output(
            output_data,
            output_shape,
            conf_threshold,
            nms_threshold,
            scale,
            pad_w,
            pad_h,
            frame.cols,
            frame.rows);

        for (const auto& det : detections) {
            cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);

            std::string label =
                "cls:" + std::to_string(det.class_id) +
                " " + cv::format("%.2f", det.score);

            int baseline = 0;
            cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            int x = det.box.x;
            int y = std::max(det.box.y - 4, ts.height + 4);

            cv::rectangle(
                frame,
                cv::Rect(x, y - ts.height - 6, ts.width + 6, ts.height + 6),
                cv::Scalar(0, 255, 0),
                cv::FILLED);

            cv::putText(
                frame, label, cv::Point(x + 3, y - 3),
                cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(0, 0, 0), 1);
        }

        cv::imshow("YOLO26 + ONNX Runtime", frame);
        int key = cv::waitKey(1);
        if (key == 27 || key == 'q') break;
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
