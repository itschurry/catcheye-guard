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
#include "guard/processor.hpp"

namespace catcheye::guard {
namespace {

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
        } else if (arg == "--rtsp") {
            options.publish_results = true;
            options.render_preview = false;
            if (i + 1 < args.size() && args[i + 1][0] != '-') {
                ++i;
                options.rtsp_port = std::stoi(args[i]);
            }
        } else if (arg == "--rtsp-with-preview") {
            options.publish_results = true;
            options.render_preview = true;
        } else if (arg == "--headless") {
            options.render_preview = false;
        } else if (arg == "--num-threads" && i + 1 < args.size()) {
            ++i;
            options.num_threads = std::stoi(args[i]);
        } else if (arg == "--image" || arg == "--video" || arg == "--camera-pipeline" || arg == "--num-threads") {
            throw std::runtime_error("missing value for flag: " + arg);
        } else if (is_input_mode(arg)) {
            throw std::runtime_error("input mode flags are mutually exclusive");
        } else {
            options.positional_args.push_back(arg);
        }
    }

    if ((options.input.type == catcheye::input::InputSourceType::ImageFile ||
         options.input.type == catcheye::input::InputSourceType::VideoFile) &&
        !options.input.camera_pipeline.empty()) {
        throw std::runtime_error("--camera-pipeline is only valid with --camera");
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
    bootstrap.processor_config.roi_auto_reload = options.input.type == catcheye::input::InputSourceType::Camera;
    bootstrap.processor_config.roi_config_path = loaded_roi_config.path;
    bootstrap.processor_config.roi_config = loaded_roi_config.config;

    bootstrap.runtime_config.window_name = "CatchEye Person Guard";
    bootstrap.runtime_config.render_preview = options.render_preview;
    bootstrap.runtime_config.process_every_n_frames = 2;
    bootstrap.publish_results = options.publish_results;
    bootstrap.publisher_config.port = options.rtsp_port;

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
        log->info("catcheye-guard starting (ROI='{}', preview={}, rtsp={})", bootstrap.processor_config.roi_config_path,
                  bootstrap.runtime_config.render_preview, bootstrap.publish_results);
    }

    std::unique_ptr<catcheye::transport::ResultPublisher> publisher;
    if (bootstrap.publish_results) {
        publisher = std::make_unique<catcheye::transport::RtspPublisher>(bootstrap.publisher_config);
    }

    auto processor = std::make_unique<catcheye::GuardProcessor>(std::move(bootstrap.processor_config));
    catcheye::runtime::FrameProcessingRunner runner(std::move(bootstrap.runtime_config), std::move(bootstrap.source), std::move(processor),
                                                    std::move(publisher));
    return runner.run();
}

} // namespace catcheye::guard
