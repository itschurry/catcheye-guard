#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "catcheye/core/pipeline.hpp"
#include "catcheye/utils/logger.hpp"

namespace {

std::string resolve_default_model_path(const char* executable_path, const std::string& relative_path) {
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    candidates.emplace_back(fs::current_path() / "models" / relative_path);

    if (executable_path != nullptr && *executable_path != '\0') {
        const fs::path executable = fs::absolute(executable_path);
        const fs::path executable_dir = executable.parent_path();
        candidates.emplace_back(executable_dir / ".." / "models" / relative_path);
        candidates.emplace_back(executable_dir / "models" / relative_path);
    }

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

    if (argc > 1) {
        config.detector.param_path = argv[1];
    }
    if (argc > 2) {
        config.detector.bin_path = argv[2];
    }
    if (argc > 3) {
        config.detector.metadata_path = argv[3];
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

    if (const auto log = catcheye::logger()) {
        log->info("catcheye-guard starting");
    }

    catcheye::Pipeline pipeline(config);
    const int exit_code = pipeline.run();
    if (const auto log = catcheye::logger()) {
        log->info("catcheye-guard exiting with code {}", exit_code);
    }
    catcheye::shutdown_logging();
    return exit_code;
}
