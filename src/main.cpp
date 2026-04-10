#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "catcheye/core/pipeline.hpp"
#include "catcheye/guard/roi/roi_repository.hpp"
#include "catcheye/guard/roi/roi_validation.hpp"
#include "catcheye/utils/logger.hpp"

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

} // namespace

int main(int argc, char** argv) {
    catcheye::initialize_logging("catcheye_guard", "log");

    catcheye::PipelineConfig config;
    config.camera.pipeline = "libcamerasrc ! "
                             "video/x-raw,width=640,height=480,framerate=15/1,format=NV12 ! "
                             "videoflip video-direction=vert ! "
                             "videoconvert ! "
                             "video/x-raw,format=BGR ! "
                             "appsink drop=true max-buffers=1 sync=false";

    config.detector.param_path = resolve_default_model_path(argv[0], "yolo26n_ncnn_model/model.ncnn.param");
    config.detector.bin_path = resolve_default_model_path(argv[0], "yolo26n_ncnn_model/model.ncnn.bin");
    config.detector.metadata_path = resolve_default_model_path(argv[0], "yolo26n_ncnn_model/metadata.yaml");
    std::string roi_config_path = resolve_default_model_path(argv[0], "roi_cam_default.json");

    // Parse named flags first
    std::vector<std::string> positional_args;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--stream") {
            config.stream_preview = true;
            config.render_preview = false;
            // Optional: next arg may be a socket path (if not another flag)
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                config.stream_config.socket_path = argv[++i];
            }
        } else if (arg == "--stream-with-preview") {
            config.stream_preview = true;
            config.render_preview = true;
        } else if (arg == "--num-threads" && i + 1 < argc) {
            try {
                config.detector.num_threads = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "invalid --num-threads value: " << argv[i] << '\n';
                catcheye::shutdown_logging();
                return 1;
            }
        } else {
            positional_args.push_back(arg);
        }
    }

    // Apply positional args: [param_path] [bin_path] [metadata_path] [roi_config_path]
    if (positional_args.size() > 0) {
        config.detector.param_path = positional_args[0];
    }
    if (positional_args.size() > 1) {
        config.detector.bin_path = positional_args[1];
    }
    if (positional_args.size() > 2) {
        config.detector.metadata_path = positional_args[2];
    }
    if (positional_args.size() > 3) {
        roi_config_path = positional_args[3];
    }

    if (config.detector.param_path.empty() || config.detector.bin_path.empty()) {
        if (const auto log = catcheye::logger()) {
            log->error("model paths are required");
        } else {
            std::cerr << "Model paths are required.\n";
        }
        catcheye::shutdown_logging();
        return 1;
    }

    const auto roi_parse_result = catcheye::guard::roi::RoiRepository::load_from_file(roi_config_path);
    if (!roi_parse_result.success) {
        if (const auto log = catcheye::logger()) {
            log->error("failed to load ROI config '{}'", roi_config_path);
            for (const std::string& error : roi_parse_result.errors) {
                log->error("ROI parse error: {}", error);
            }
        } else {
            std::cerr << "Failed to load ROI config: " << roi_config_path << '\n';
        }
        catcheye::shutdown_logging();
        return 1;
    }

    const auto roi_validation_result = catcheye::guard::roi::validate_camera_roi_config(roi_parse_result.config);
    if (!roi_validation_result.valid) {
        if (const auto log = catcheye::logger()) {
            log->error("ROI config '{}' failed validation", roi_config_path);
            for (const auto& issue : roi_validation_result.issues) {
                log->error("ROI validation issue: zone_index={}, point_index={}, message={}", issue.zone_index, issue.point_index,
                           issue.message);
            }
        }
        catcheye::shutdown_logging();
        return 1;
    }

    config.roi_enabled = true;
    config.roi_auto_reload = true;
    config.roi_config_path = roi_config_path;
    config.roi_config = roi_parse_result.config;

    if (const auto log = catcheye::logger()) {
        log->info("catcheye-guard starting (ROI='{}', stream={}, preview={})", roi_config_path, config.stream_preview,
                  config.render_preview);
    }

    catcheye::Pipeline pipeline(config);
    const int exit_code = pipeline.run();
    if (const auto log = catcheye::logger()) {
        log->info("catcheye-guard exiting with code {}", exit_code);
    }
    catcheye::shutdown_logging();
    return exit_code;
}
