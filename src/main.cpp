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
                             "video/x-raw,width=1280,height=720,framerate=30/1,format=NV12 ! "
                             "videoflip video-direction=vert ! "
                             "videoconvert ! "
                             "video/x-raw,format=BGR ! "
                             "appsink drop=true max-buffers=1 sync=false";

    config.detector.param_path = resolve_default_model_path(argv[0], "yolo26n_ncnn_model/model.ncnn.param");
    config.detector.bin_path = resolve_default_model_path(argv[0], "yolo26n_ncnn_model/model.ncnn.bin");
    config.detector.metadata_path = resolve_default_model_path(argv[0], "yolo26n_ncnn_model/metadata.yaml");
    std::string roi_config_path = resolve_default_model_path(argv[0], "roi_cam_default.json");

    if (argc > 1) {
        config.detector.param_path = argv[1];
    }
    if (argc > 2) {
        config.detector.bin_path = argv[2];
    }
    if (argc > 3) {
        config.detector.metadata_path = argv[3];
    }
    if (argc > 4) {
        roi_config_path = argv[4];
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
                log->error(
                    "ROI validation issue: zone_index={}, point_index={}, message={}",
                    issue.zone_index,
                    issue.point_index,
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
        log->info("catcheye-guard starting with ROI config '{}'", roi_config_path);
    }

    catcheye::Pipeline pipeline(config);
    const int exit_code = pipeline.run();
    if (const auto log = catcheye::logger()) {
        log->info("catcheye-guard exiting with code {}", exit_code);
    }
    catcheye::shutdown_logging();
    return exit_code;
}
