#include "guard/app.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "catcheye/roi/roi_repository.hpp"
#include "catcheye/roi/roi_validation.hpp"
#include "catcheye/runtime/frame_processing_runner.hpp"
#include "catcheye/transport/rtsp_publisher.hpp"
#include "catcheye/utils/logger.hpp"
#include "guard/http_roi_server.hpp"
#include "guard/processor.hpp"

namespace catcheye::guard {
namespace {

const char* publisher_name(PublisherType type) {
    switch (type) {
    case PublisherType::Rtsp:
        return "rtsp";
    case PublisherType::WebSocket:
        return "websocket";
    case PublisherType::None:
        return "local";
    }

    return "unknown";
}

std::string resolve_default_model_path(const char* executable_path, const std::string& relative_path) {
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    if (executable_path != nullptr && *executable_path != '\0') {
        const fs::path executable = fs::absolute(executable_path);
        const fs::path executable_dir = executable.parent_path();
        candidates.emplace_back(executable_dir / ".." / "models" / relative_path);
        candidates.emplace_back(executable_dir / "models" / relative_path);
    }
    candidates.emplace_back(fs::current_path() / "models" / relative_path);

    for (const fs::path& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate.lexically_normal().string();
        }
    }

    return (fs::current_path() / "models" / relative_path).lexically_normal().string();
}

bool is_input_mode(const std::string& arg) {
    return arg == "--image" || arg == "--video" || arg == "--camera";
}

std::string describe_runtime_mode(const AppOptions& options)
{
    const bool rtsp_enabled = options.publisher_type == PublisherType::Rtsp;
    const bool websocket_enabled = options.publisher_type == PublisherType::WebSocket;
    const char* output_name = rtsp_enabled ? "rtsp output" : (websocket_enabled ? "websocket output" : "local output");

    if (options.input.type == catcheye::input::InputSourceType::Camera) {
        if (!options.input.camera_pipeline.empty()) {
            return std::string("csi camera + gstreamer source + ") + output_name;
        }
        if (!options.input.camera_device.empty()) {
            return std::string("usb camera + gstreamer source + ") + output_name;
        }
        return std::string("csi camera + libcamera source + ") + output_name;
    }

    const char* input_name =
        options.input.type == catcheye::input::InputSourceType::ImageFile
        ? "image file"
        : "video file";
    return std::string(input_name) + " + gstreamer source + " + output_name;
}

} // namespace

AppOptions parse_app_options(int argc, char** argv) {
    AppOptions options;

    const std::span<char* const> args(argv, static_cast<std::size_t>(argc));
    bool input_mode_selected = false;

    for (std::size_t i = 1; i < args.size(); ++i) {
        std::string arg(args[i]);
        if (arg == "--image" && i + 1 < args.size()) {
            if (input_mode_selected) {
                throw std::runtime_error("input mode flags are mutually exclusive");
            }
            input_mode_selected = true;
            options.input.type = catcheye::input::InputSourceType::ImageFile;
            options.input.uri = args[++i];
        } else if (arg == "--video" && i + 1 < args.size()) {
            if (input_mode_selected) {
                throw std::runtime_error("input mode flags are mutually exclusive");
            }
            input_mode_selected = true;
            options.input.type = catcheye::input::InputSourceType::VideoFile;
            options.input.uri = args[++i];
        } else if (arg == "--camera") {
            if (input_mode_selected) {
                throw std::runtime_error("input mode flags are mutually exclusive");
            }
            input_mode_selected = true;
            options.input.type = catcheye::input::InputSourceType::Camera;
        } else if (arg == "--camera-pipeline" && i + 1 < args.size()) {
            options.input.camera_pipeline = args[++i];
        } else if (arg == "--camera-device" && i + 1 < args.size()) {
            options.input.camera_device = args[++i];
        } else if (arg == "--camera-width" && i + 1 < args.size()) {
            options.input.camera_width = std::stoi(args[++i]);
        } else if (arg == "--camera-height" && i + 1 < args.size()) {
            options.input.camera_height = std::stoi(args[++i]);
        } else if (arg == "--rtsp") {
            if (options.publisher_type != PublisherType::None) {
                throw std::runtime_error("only one publisher can be selected at a time");
            }
            options.publisher_type = PublisherType::Rtsp;
            if (i + 1 < args.size() && args[i + 1][0] != '-') {
                ++i;
                options.rtsp_port = std::stoi(args[i]);
            }
        } else if (arg == "--ws") {
            if (options.publisher_type != PublisherType::None) {
                throw std::runtime_error("only one publisher can be selected at a time");
            }
            options.publisher_type = PublisherType::WebSocket;
            if (i + 1 < args.size() && args[i + 1][0] != '-') {
                ++i;
                options.websocket_port = std::stoi(args[i]);
            }
        } else if (arg == "--rtsp-with-preview") {
            throw std::runtime_error("--rtsp-with-preview is no longer supported; use --rtsp");
        } else if (arg == "--ws-with-preview") {
            throw std::runtime_error("--ws-with-preview is no longer supported; use --ws");
        } else if (arg == "--headless") {
            throw std::runtime_error("--headless is no longer needed; preview output is not supported");
        } else if (arg == "--num-threads" && i + 1 < args.size()) {
            ++i;
            options.num_threads = std::stoi(args[i]);
        } else if (arg == "--http-port" && i + 1 < args.size()) {
            ++i;
            options.http_port = std::stoi(args[i]);
        } else if (arg == "--image" || arg == "--video" || arg == "--camera-pipeline" ||
                   arg == "--camera-device" || arg == "--ws" ||
                   arg == "--camera-width" || arg == "--camera-height" || arg == "--num-threads" ||
                   arg == "--http-port") {
            throw std::runtime_error("missing value for flag: " + arg);
        } else if (is_input_mode(arg)) {
            throw std::runtime_error("input mode flags are mutually exclusive");
        } else {
            options.positional_args.push_back(arg);
        }
    }

    if ((options.input.type == catcheye::input::InputSourceType::ImageFile ||
         options.input.type == catcheye::input::InputSourceType::VideoFile) &&
        (!options.input.camera_pipeline.empty() || !options.input.camera_device.empty())) {
        throw std::runtime_error("--camera-pipeline and --camera-device are only valid with --camera");
    }

    if (!options.input.camera_pipeline.empty() &&
        (options.input.camera_width != 1280 || options.input.camera_height != 720)) {
        throw std::runtime_error("--camera-width and --camera-height are not supported with --camera-pipeline");
    }

    if (!options.input.camera_pipeline.empty() && !options.input.camera_device.empty()) {
        throw std::runtime_error("--camera-pipeline and --camera-device cannot be used together");
    }

    if ((options.input.type == catcheye::input::InputSourceType::ImageFile ||
         options.input.type == catcheye::input::InputSourceType::VideoFile) &&
        (options.input.camera_width != 1280 || options.input.camera_height != 720)) {
        throw std::runtime_error("--camera-width and --camera-height are only valid with --camera");
    }

    if (options.input.camera_width <= 0 || options.input.camera_height <= 0) {
        throw std::runtime_error("camera dimensions must be positive integers");
    }
    if (options.http_port <= 0) {
        throw std::runtime_error("HTTP port must be a positive integer");
    }

    if ((options.input.camera_width % 2) != 0 || (options.input.camera_height % 2) != 0) {
        throw std::runtime_error("camera dimensions must be even for NV12/RTSP output");
    }

    if ((options.input.type == catcheye::input::InputSourceType::ImageFile ||
         options.input.type == catcheye::input::InputSourceType::VideoFile) &&
        options.input.uri.empty()) {
        throw std::runtime_error("input path is required for --image or --video");
    }

    return options;
}

DefaultPaths resolve_default_paths(const char* executable_path) {
    return DefaultPaths{
        .param_path = resolve_default_model_path(executable_path, "yolo26n_ncnn_model/model.ncnn.param"),
        .bin_path = resolve_default_model_path(executable_path, "yolo26n_ncnn_model/model.ncnn.bin"),
        .metadata_path = resolve_default_model_path(executable_path, "yolo26n_ncnn_model/metadata.yaml"),
        .roi_config_path = resolve_default_model_path(executable_path, "roi_cam_default.json"),
    };
}

LoadedRoiConfig load_and_validate_roi_config(const std::string& roi_config_path) {
    const auto roi_parse_result = catcheye::roi::RoiRepository::load_from_file(roi_config_path);
    if (!roi_parse_result.success) {
        throw std::runtime_error("failed to load ROI config: " + roi_config_path);
    }

    const auto roi_validation_result = catcheye::roi::validate_camera_roi_config(roi_parse_result.config);
    if (!roi_validation_result.valid) {
        throw std::runtime_error("ROI config failed validation: " + roi_config_path);
    }

    return LoadedRoiConfig{
        .path = roi_config_path,
        .config = roi_parse_result.config,
    };
}

AppBootstrap build_app_bootstrap(const AppOptions& options, const DefaultPaths& default_paths, const LoadedRoiConfig& loaded_roi_config) {
    AppBootstrap bootstrap;

    // ── NCNN 백엔드 설정 ──────────────────────────────────────
    auto& ncnn_cfg = bootstrap.processor_config.detector.ncnn;
    ncnn_cfg.param_path = default_paths.param_path;
    ncnn_cfg.bin_path = default_paths.bin_path;
    ncnn_cfg.metadata_path = default_paths.metadata_path;
    ncnn_cfg.num_threads = options.num_threads;

    // positional args override (기존 동작 유지)
    if (!options.positional_args.empty()) ncnn_cfg.param_path = options.positional_args[0];
    if (options.positional_args.size() > 1) ncnn_cfg.bin_path = options.positional_args[1];
    if (options.positional_args.size() > 2) ncnn_cfg.metadata_path = options.positional_args[2];

    // ── ROI / runtime 설정 (기존과 동일) ─────────────────────
    bootstrap.processor_config.roi_enabled = true;
    bootstrap.processor_config.roi_config_path = loaded_roi_config.path;
    bootstrap.processor_config.roi_config = loaded_roi_config.config;

    bootstrap.runtime_config.process_every_n_frames = 2;
    bootstrap.publisher_type = options.publisher_type;
    bootstrap.rtsp_publisher_config.port = options.rtsp_port;
    bootstrap.rtsp_publisher_config.width = options.input.camera_width;
    bootstrap.rtsp_publisher_config.height = options.input.camera_height;
    bootstrap.websocket_publisher_config.port = options.websocket_port;
    bootstrap.http_roi_server_config.port = options.http_port;

    if (ncnn_cfg.param_path.empty() || ncnn_cfg.bin_path.empty()) {
        throw std::runtime_error("NCNN model paths are required");
    }

    bootstrap.source = catcheye::input::create_frame_source(options.input);
    return bootstrap;
}

int run_app(int argc, char** argv) {
    const AppOptions options = parse_app_options(argc, argv);
    const DefaultPaths default_paths = resolve_default_paths(argv[0]);
    const std::string roi_config_path = options.positional_args.size() > 3 ? options.positional_args[3] : default_paths.roi_config_path;
    const LoadedRoiConfig loaded_roi_config = load_and_validate_roi_config(roi_config_path);
    AppBootstrap bootstrap = build_app_bootstrap(options, default_paths, loaded_roi_config);

    if (const auto log = logger()) {
        log->info("catcheye-guard starting (mode='{}', ROI='{}', publisher='{}', http_port={})",
                  describe_runtime_mode(options),
                  bootstrap.processor_config.roi_config_path,
                  publisher_name(bootstrap.publisher_type),
                  bootstrap.http_roi_server_config.port);
    }

    std::unique_ptr<catcheye::transport::ResultPublisher> publisher;
    if (bootstrap.publisher_type == PublisherType::Rtsp) {
        publisher = std::make_unique<catcheye::transport::RtspPublisher>(bootstrap.rtsp_publisher_config);
    } else if (bootstrap.publisher_type == PublisherType::WebSocket) {
        publisher = std::make_unique<catcheye::transport::WebSocketPublisher>(bootstrap.websocket_publisher_config);
    }

    const std::string http_roi_config_path = bootstrap.processor_config.roi_config_path;
    auto processor = std::make_unique<catcheye::GuardProcessor>(std::move(bootstrap.processor_config));
    auto* processor_ptr = processor.get();
    HttpRoiServer roi_server(
        bootstrap.http_roi_server_config,
        http_roi_config_path,
        processor_ptr);
    if (!roi_server.start()) {
        throw std::runtime_error("failed to start ROI HTTP API server");
    }

    catcheye::runtime::FrameProcessingRunner runner(std::move(bootstrap.runtime_config), std::move(bootstrap.source), std::move(processor),
                                                    std::move(publisher));
    const int exit_code = runner.run();
    roi_server.stop();
    return exit_code;
}

} // namespace catcheye::guard
