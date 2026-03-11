#include <opencv2/opencv.hpp>
#include <net.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <yaml-cpp/yaml.h>

struct Detection {
    int class_id;
    float score;
    cv::Rect box;
};

static std::map<int, std::string> load_class_names(const std::string& yaml_path)
{
    std::map<int, std::string> class_names;

    try {
        YAML::Node root = YAML::LoadFile(yaml_path);

        if (!root["names"]) {
            std::cerr << "No 'names' field in metadata.yaml\n";
            return class_names;
        }

        YAML::Node names = root["names"];
        for (auto it = names.begin(); it != names.end(); ++it) {
            int class_id = it->first.as<int>();
            std::string class_name = it->second.as<std::string>();
            class_names[class_id] = class_name;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load metadata.yaml: " << e.what() << std::endl;
    }

    return class_names;
}

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
    int resized_w = (int)std::round(orig_w * scale);
    int resized_h = (int)std::round(orig_h * scale);

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

static void draw_detections(
    cv::Mat& frame,
    const std::vector<Detection>& detections,
    const std::map<int, std::string>& class_names)
{
    for (const auto& det : detections) {
        cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);

        std::string class_name = "cls:" + std::to_string(det.class_id);
        auto it = class_names.find(det.class_id);
        if (it != class_names.end()) {
            class_name = it->second;
        }

        std::string label = class_name + " " + cv::format("%.2f", det.score);

        int baseline = 0;
        cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

        int x = det.box.x;
        int y = std::max(det.box.y - 4, ts.height + 4);

        cv::rectangle(
            frame,
            cv::Rect(x, y - ts.height - 6, ts.width + 6, ts.height + 6),
            cv::Scalar(0, 255, 0),
            cv::FILLED
        );

        cv::putText(frame, label, cv::Point(x + 3, y - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(0, 0, 0), 1);
    }
}

// 아주 단순한 decode 예제
// output shape 확인 후 여기를 맞추면 됨
static std::vector<Detection> decode_yolo_ncnn(
    const ncnn::Mat& out,
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

    std::cout << "NCNN output dims=" << out.dims
              << " w=" << out.w
              << " h=" << out.h
              << " c=" << out.c << std::endl;

    if (out.dims != 2) {
        std::cerr << "Unexpected output dims: " << out.dims << std::endl;
        return results;
    }

    // out: [84, 8400] 형태로 해석
    // h = 84 attributes
    // w = 8400 candidates
    const int num_rows = out.w;  // 8400 candidates
    const int num_cols = out.h;  // 84 attributes

    if (num_cols < 6) {
        std::cerr << "Unexpected num_cols: " << num_cols << std::endl;
        return results;
    }

    const int left = pad_w / 2;
    const int top  = pad_h / 2;

    // 각 attribute row 포인터
    const float* row_cx = out.row(0);
    const float* row_cy = out.row(1);
    const float* row_w  = out.row(2);
    const float* row_h  = out.row(3);

    // for (int i = 0; i < 5; ++i) {
    //     float cx = row_cx[i];
    //     float cy = row_cy[i];
    //     float w  = row_w[i];
    //     float h  = row_h[i];

    //     float best_score = 0.f;
    //     int best_class = -1;

    //     for (int c = 4; c < num_cols; ++c) {
    //         const float* row_cls = out.row(c);
    //         float score = row_cls[i];
    //         if (score > best_score) {
    //             best_score = score;
    //             best_class = c - 4;
    //         }
    //     }

    //     std::cout << "cand[" << i << "] "
    //               << "cx=" << cx << ", cy=" << cy
    //               << ", w=" << w << ", h=" << h
    //               << ", cls=" << best_class
    //               << ", score=" << best_score << std::endl;
    // }

    for (int i = 0; i < num_rows; ++i) {
        float cx = row_cx[i];
        float cy = row_cy[i];
        float w  = row_w[i];
        float h  = row_h[i];

        int best_class = -1;
        float best_score = 0.f;

        for (int c = 4; c < num_cols; ++c) {
            const float* row_cls = out.row(c);
            float score = row_cls[i];

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

        int bw = std::max(0, (int)(x2 - x1));
        int bh = std::max(0, (int)(y2 - y1));
        if (bw <= 1 || bh <= 1) continue;

        class_ids.push_back(best_class);
        scores.push_back(best_score);
        boxes.emplace_back((int)x1, (int)y1, bw, bh);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, conf_threshold, nms_threshold, indices);

    for (int idx : indices) {
        results.push_back({class_ids[idx], scores[idx], boxes[idx]});
    }

    return results;
}

int main()
{
    const std::string pipeline =
        "libcamerasrc ! "
        "video/x-raw,width=1280,height=720,framerate=30/1,format=NV12 ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink drop=true max-buffers=1 sync=false";

    const std::string param_path = "yolo26n_ncnn_model/model.ncnn.param";
    const std::string bin_path   = "yolo26n_ncnn_model/model.ncnn.bin";

    const int input_size = 640;
    const float conf_threshold = 0.25f;
    const float nms_threshold = 0.45f;

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera pipeline.\n";
        return 1;
    }

    ncnn::Net net;
    net.opt.use_vulkan_compute = false;  // Pi5에서는 일단 CPU부터
    net.opt.num_threads = 2;

    if (net.load_param(param_path.c_str()) != 0) {
        std::cerr << "Failed to load param: " << param_path << "\n";
        return 1;
    }

    if (net.load_model(bin_path.c_str()) != 0) {
        std::cerr << "Failed to load bin: " << bin_path << "\n";
        return 1;
    }

    std::cout << "NCNN model loaded.\n";

    std::map<int, std::string> class_names =
    load_class_names("yolo26n_ncnn_model/metadata.yaml");

    std::cout << "Loaded " << class_names.size() << " class names\n";

    cv::Mat frame;
    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Failed to read frame.\n";
            break;
        }

        float scale = 1.0f;
        int pad_w = 0;
        int pad_h = 0;
        cv::Mat input_img = letterbox(frame, input_size, input_size, scale, pad_w, pad_h);

        cv::Mat rgb;
        cv::cvtColor(input_img, rgb, cv::COLOR_BGR2RGB);

        ncnn::Mat in = ncnn::Mat::from_pixels_resize(
            rgb.data,
            ncnn::Mat::PIXEL_RGB,
            rgb.cols,
            rgb.rows,
            input_size,
            input_size);

        const float norm_vals[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
        in.substract_mean_normalize(nullptr, norm_vals);

        ncnn::Extractor ex = net.create_extractor();
        ex.input("in0", in);   // 여기 이름은 모델에 따라 다를 수 있음

        ncnn::Mat out;
        ex.extract("out0", out);  // 여기 이름도 모델에 따라 다를 수 있음
	

        auto detections = decode_yolo_ncnn(
            out,
            conf_threshold,
            nms_threshold,
            scale,
            pad_w,
            pad_h,
            frame.cols,
            frame.rows);
	std::vector<Detection> person_detections;
        for (const auto& det : detections) {
            if (det.class_id == 0) {
                person_detections.push_back(det);
            }
        }


        draw_detections(frame, person_detections, class_names);

        cv::imshow("YOLO26 + NCNN", frame);
        int key = cv::waitKey(1);
        if (key == 27 || key == 'q') break;
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
