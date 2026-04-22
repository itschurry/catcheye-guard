#pragma once

#include <memory>
#include <string>
#include <vector>

#include "catcheye/input/frame_source.hpp"
#include "catcheye/roi/camera_roi_config.hpp"
#include "catcheye/runtime/frame_processing_runner.hpp"
#include "catcheye/transport/rtsp_publisher.hpp"
#include "guard/processor_config.hpp"

namespace catcheye::guard {

struct AppOptions {
    catcheye::input::InputSourceConfig input;
    bool publish_results = false;
    int rtsp_port = 8554;
    int num_threads = 2;
    std::vector<std::string> positional_args;
};

struct DefaultPaths {
    std::string param_path;
    std::string bin_path;
    std::string metadata_path;
    std::string roi_config_path;
};

struct LoadedRoiConfig {
    std::string path;
    catcheye::roi::CameraRoiConfig config;
};

struct AppBootstrap {
    GuardProcessorConfig processor_config;
    catcheye::runtime::RuntimeConfig runtime_config;
    catcheye::transport::RtspPublisherConfig publisher_config;
    bool publish_results = false;
    std::unique_ptr<catcheye::input::FrameSource> source;
};

AppOptions parse_app_options(int argc, char** argv);
DefaultPaths resolve_default_paths(const char* executable_path);
LoadedRoiConfig load_and_validate_roi_config(const std::string& roi_config_path);
AppBootstrap build_app_bootstrap(const AppOptions& options, const DefaultPaths& default_paths, const LoadedRoiConfig& loaded_roi_config);

int run_app(int argc, char** argv);

} // namespace catcheye::guard
