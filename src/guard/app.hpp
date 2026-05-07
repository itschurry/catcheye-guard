#pragma once

#include <memory>
#include <string>
#include <vector>

#include "catcheye/input/frame_source.hpp"
#include "catcheye/roi/camera_roi_config.hpp"
#include "catcheye/runtime/frame_processing_runner.hpp"
#include "guard/http_roi_server.hpp"
#include "catcheye/transport/rtsp_publisher.hpp"
#include "catcheye/transport/websocket_publisher.hpp"
#include "guard/processor_config.hpp"

namespace catcheye::guard {

enum class PublisherType {
    None,
    Rtsp,
    WebSocket,
};

struct AppOptions {
    catcheye::input::InputSourceConfig input;
    PublisherType publisher_type = PublisherType::None;
    int rtsp_port = 8554;
    int websocket_port = 8080;
    int http_port = 8090;
    int num_threads = 2;
    int roi_alert_gpio = 14;
    int roi_alert_pulse_ms = 100;
    bool roi_alert_active_low = false;
    bool viewer_only = false;
    int pallet_class_id = 1;
    std::string gpio_chip_path = "/dev/gpiochip4";
    catcheye::DetectorBackend detector_backend = catcheye::DetectorBackend::Ncnn;
    std::string hef_path;
    std::string metadata_path;
    std::string pallet_roi_config_path;
    std::vector<std::string> positional_args;
};

struct DefaultPaths {
    std::string param_path;
    std::string bin_path;
    std::string metadata_path;
    std::string hef_path;
    std::string roi_config_path;
    std::string pallet_roi_config_path;
};

struct LoadedRoiConfig {
    bool loaded = false;
    std::string path;
    catcheye::roi::CameraRoiConfig config;
};

struct AppBootstrap {
    GuardProcessorConfig processor_config;
    catcheye::runtime::RuntimeConfig runtime_config;
    PublisherType publisher_type = PublisherType::None;
    catcheye::transport::RtspPublisherConfig rtsp_publisher_config;
    catcheye::transport::WebSocketPublisherConfig websocket_publisher_config;
    HttpRoiServerConfig http_roi_server_config;
    std::unique_ptr<catcheye::input::FrameSource> source;
};

AppOptions parse_app_options(int argc, char** argv);
DefaultPaths resolve_default_paths(const char* executable_path);
LoadedRoiConfig load_and_validate_roi_config(const std::string& roi_config_path);
AppBootstrap build_app_bootstrap(
    const AppOptions& options,
    const DefaultPaths& default_paths,
    const LoadedRoiConfig& loaded_roi_config,
    const LoadedRoiConfig& loaded_pallet_roi_config);

int run_app(int argc, char** argv);

} // namespace catcheye::guard
