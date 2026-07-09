#include "guard/app.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "catcheye/roi/roi_repository.hpp"
#include "catcheye/roi/roi_validation.hpp"
#include "catcheye/runtime/frame_processing_runner.hpp"
#include "catcheye/utils/logger.hpp"
#include "guard/camera_properties.hpp"
#include "guard/http_api_server.hpp"
#include "guard/processor.hpp"
#include "guard/recording_controller.hpp"

namespace catcheye::guard {
namespace {

constexpr std::string_view DEFAULT_CAMERA_PIPELINE =
    "libcamerasrc ! video/x-raw,width=2304,height=1296,framerate=15/1,format=NV12 ! queue leaky=downstream max-size-buffers=1 ! videoflip method=rotate-180";
constexpr int DEFAULT_CAMERA_WIDTH = 2304;
constexpr int DEFAULT_CAMERA_HEIGHT = 1296;

void print_usage() {
    std::cout << "Usage:\n"
              << "  catcheye-guard [options]\n"
              << "  catcheye-guard [options] [roi.json]\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help                  Show this help\n"
              << "  --camera                    Use camera input (default)\n"
              << "  --image <path>              Use image file input\n"
              << "  --video <path>              Use video file input\n"
              << "  --camera-pipeline <pipe>    Use explicit GStreamer camera pipeline\n"
              << "  --camera-device <path>      Use camera device path\n"
              << "  --camera-width <pixels>     Camera width (default: 2304 for default CSI pipeline)\n"
              << "  --camera-height <pixels>    Camera height (default: 1296 for default CSI pipeline)\n"
              << "  --viewer-only               Disable detection; valid only with --camera and --ws\n"
              << "  --ws [port]                 Publish frames over WebSocket (default port: 8080)\n"
              << "  --http-port <port>          HTTP API port (default: 8090)\n"
              << "  --hef <path>                Hailo HEF model path\n"
              << "  --metadata <path>           Detector metadata YAML path\n"
              << "  --pallet-roi <path>         Pallet ROI config path\n"
              << "  --pallet-class-id <id>      Pallet class id (default: 1)\n"
              << "  --roi-alert-gpio <line>     ROI alert GPIO line; -1 disables output (default: 14)\n"
              << "  --gpio-chip <path>          GPIO chip path (default: /dev/gpiochip4)\n"
              << "  --roi-alert-active-low      Drive ROI alert GPIO active-low\n"
              << "  --person-roi-alert-disable-gpio <line>  Disable person ROI alert while GPIO input is active\n"
              << "  --person-roi-alert-disable-active-low   Treat person ROI alert disable input as active-low\n"
              << "  --person-roi-alert-disable-debounce-ms <ms>  GPIO input debounce duration (default: 200)\n"
              << "  --recording-dir <path>      Preview recording directory (default: recordings)\n"
              << "  --camera-properties <path>  RGB camera properties JSON path (default: config/camera_properties.json)\n"
              << "\n"
              << "Examples:\n"
              << "  catcheye-guard --help\n"
              << "  catcheye-guard --camera --ws\n"
              << "  catcheye-guard --camera --ws --hef models/yolo26m.hef --metadata models/metadata.yaml\n"
              << "  catcheye-guard --image samples/frame.jpg --ws --hef models/yolo26m.hef --metadata models/metadata.yaml\n";
}

const char* publisher_name(PublisherType type) {
    switch (type) {
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

std::string resolve_default_config_path(const char* executable_path, const std::string& relative_path) {
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    if (executable_path != nullptr && *executable_path != '\0') {
        const fs::path executable = fs::absolute(executable_path);
        const fs::path executable_dir = executable.parent_path();
        candidates.emplace_back(executable_dir / ".." / "config" / relative_path);
        candidates.emplace_back(executable_dir / "config" / relative_path);
    }
    candidates.emplace_back(fs::current_path() / "config" / relative_path);

    for (const fs::path& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate.lexically_normal().string();
        }
    }

    return (fs::current_path() / "config" / relative_path).lexically_normal().string();
}

bool is_input_mode(std::string_view arg) {
    return arg == "--image" || arg == "--video" || arg == "--camera";
}

std::string_view read_required_value(std::span<char* const> args, std::size_t& index, std::string_view flag) {
    if (index + 1 >= args.size()) {
        throw std::invalid_argument(std::string(flag) + " requires a value");
    }
    return args[++index];
}

std::string describe_runtime_mode(const AppOptions& options)
{
    const bool websocket_enabled = options.publisher_type == PublisherType::WebSocket;
    const char* output_name = websocket_enabled ? "websocket output" : "local output";
    const char* processing_name = options.viewer_only ? "viewer only" : "detection";

    if (options.input.type == catcheye::input::InputSourceType::Camera) {
        if (!options.input.camera_pipeline.empty()) {
            return std::string("csi camera + gstreamer source + ") + processing_name + " + " + output_name;
        }
        if (!options.input.camera_device.empty()) {
            return std::string("usb camera + gstreamer source + ") + processing_name + " + " + output_name;
        }
        return std::string("csi camera + libcamera source + ") + processing_name + " + " + output_name;
    }

    const char* input_name =
        options.input.type == catcheye::input::InputSourceType::ImageFile
        ? "image file"
        : "video file";
    return std::string(input_name) + " + gstreamer source + " + processing_name + " + " + output_name;
}

class CameraPropertiesFrameSource final : public catcheye::input::FrameSource {
  public:
    CameraPropertiesFrameSource(
        std::unique_ptr<catcheye::input::FrameSource> inner,
        CameraPropertyMap properties,
        std::string properties_path)
        : inner_(std::move(inner)),
          properties_(std::move(properties)),
          properties_path_(std::move(properties_path)) {}

    bool open() override
    {
        if (!inner_->open()) {
            return false;
        }
        std::string error;
        if (!apply_camera_properties(*inner_, properties_, error)) {
            std::cerr << error << " from '" << properties_path_ << "'\n";
            inner_->close();
            return false;
        }
        return true;
    }

    bool is_open() const override { return inner_->is_open(); }
    catcheye::input::FrameReadStatus read(catcheye::input::Frame& frame) override { return inner_->read(frame); }
    void close() override { inner_->close(); }
    std::string describe() const override { return inner_->describe() + " + camera-properties:" + properties_path_; }
    std::optional<std::string> property_json(std::string_view key) const override { return inner_->property_json(key); }
    bool set_bool_property(std::string_view key, bool value) override { return inner_->set_bool_property(key, value); }
    bool set_int_property(std::string_view key, int value) override { return inner_->set_int_property(key, value); }
    bool set_float_property(std::string_view key, float value) override { return inner_->set_float_property(key, value); }
    bool set_string_property(std::string_view key, std::string_view value) override
    {
        return inner_->set_string_property(key, value);
    }

  private:
    std::unique_ptr<catcheye::input::FrameSource> inner_;
    CameraPropertyMap properties_;
    std::string properties_path_;
};

} // namespace

AppOptions parse_app_options(int argc, char** argv) {
    AppOptions options;

    const std::span<char* const> args(argv, static_cast<std::size_t>(argc));
    bool input_mode_selected = false;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg(args[i]);
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
        } else if (arg == "--image") {
            if (input_mode_selected) {
                throw std::runtime_error("input mode flags are mutually exclusive");
            }
            input_mode_selected = true;
            options.input.type = catcheye::input::InputSourceType::ImageFile;
            options.input.uri = read_required_value(args, i, arg);
        } else if (arg == "--video") {
            if (input_mode_selected) {
                throw std::runtime_error("input mode flags are mutually exclusive");
            }
            input_mode_selected = true;
            options.input.type = catcheye::input::InputSourceType::VideoFile;
            options.input.uri = read_required_value(args, i, arg);
        } else if (arg == "--camera") {
            if (input_mode_selected) {
                throw std::runtime_error("input mode flags are mutually exclusive");
            }
            input_mode_selected = true;
            options.input.type = catcheye::input::InputSourceType::Camera;
        } else if (arg == "--camera-pipeline") {
            options.input.camera_pipeline = read_required_value(args, i, arg);
        } else if (arg == "--camera-device") {
            options.input.camera_device = read_required_value(args, i, arg);
        } else if (arg == "--camera-width") {
            options.input.camera_width = std::stoi(std::string(read_required_value(args, i, arg)));
        } else if (arg == "--camera-height") {
            options.input.camera_height = std::stoi(std::string(read_required_value(args, i, arg)));
        } else if (arg == "--ws") {
            if (options.publisher_type != PublisherType::None) {
                throw std::runtime_error("only one publisher can be selected at a time");
            }
            options.publisher_type = PublisherType::WebSocket;
            if (i + 1 < args.size() && args[i + 1][0] != '-') {
                options.websocket_port = std::stoi(std::string(read_required_value(args, i, arg)));
            }
        } else if (arg == "--ws-with-preview") {
            throw std::invalid_argument("--ws-with-preview is no longer supported; use --ws");
        } else if (arg == "--headless") {
            throw std::invalid_argument("--headless is no longer needed; preview output is not supported");
        } else if (arg == "--http-port") {
            options.http_port = std::stoi(std::string(read_required_value(args, i, arg)));
        } else if (arg == "--roi-alert-gpio") {
            options.roi_alert_gpio = std::stoi(std::string(read_required_value(args, i, arg)));
        } else if (arg == "--gpio-chip") {
            options.gpio_chip_path = read_required_value(args, i, arg);
        } else if (arg == "--roi-alert-active-low") {
            options.roi_alert_active_low = true;
        } else if (arg == "--person-roi-alert-disable-gpio") {
            options.person_roi_alert_disable_gpio = std::stoi(std::string(read_required_value(args, i, arg)));
        } else if (arg == "--person-roi-alert-disable-active-low") {
            options.person_roi_alert_disable_active_low = true;
        } else if (arg == "--person-roi-alert-disable-debounce-ms") {
            options.person_roi_alert_disable_debounce_ms = std::stoi(std::string(read_required_value(args, i, arg)));
        } else if (arg == "--viewer-only") {
            options.viewer_only = true;
        } else if (arg == "--hef") {
            options.hef_path = read_required_value(args, i, arg);
        } else if (arg == "--metadata") {
            options.metadata_path = read_required_value(args, i, arg);
        } else if (arg == "--pallet-roi") {
            options.pallet_roi_config_path = read_required_value(args, i, arg);
        } else if (arg == "--recording-dir") {
            options.recording_dir = read_required_value(args, i, arg);
        } else if (arg == "--camera-properties") {
            options.camera_properties_path = read_required_value(args, i, arg);
        } else if (arg == "--pallet-class-id") {
            options.pallet_class_id = std::stoi(std::string(read_required_value(args, i, arg)));
        } else if (is_input_mode(arg)) {
            throw std::runtime_error("input mode flags are mutually exclusive");
        } else if (!arg.empty() && arg.front() == '-') {
            throw std::invalid_argument("unknown option: " + std::string(arg));
        } else {
            options.positional_args.emplace_back(arg);
        }
    }

    if (options.show_help) {
        return options;
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
    if (options.roi_alert_gpio < -1) {
        throw std::runtime_error("ROI alert GPIO line must be -1 or a non-negative integer");
    }
    if (options.person_roi_alert_disable_gpio < -1) {
        throw std::runtime_error("person ROI alert disable GPIO line must be -1 or a non-negative integer");
    }
    if (options.person_roi_alert_disable_debounce_ms < 0) {
        throw std::runtime_error("person ROI alert disable debounce must be zero or a positive integer");
    }
    if (options.roi_alert_gpio >= 0 &&
        options.person_roi_alert_disable_gpio >= 0 &&
        options.roi_alert_gpio == options.person_roi_alert_disable_gpio) {
        throw std::runtime_error("ROI alert output GPIO and person ROI alert disable GPIO must use different lines");
    }
    if (options.pallet_class_id < 0) {
        throw std::runtime_error("pallet class id must be a non-negative integer");
    }
    if (options.recording_dir.empty()) {
        throw std::runtime_error("recording directory must not be empty");
    }
    if (options.camera_properties_path.empty()) {
        throw std::runtime_error("camera properties path must not be empty");
    }

    if ((options.input.camera_width % 2) != 0 || (options.input.camera_height % 2) != 0) {
        throw std::runtime_error("camera dimensions must be even for NV12 output");
    }

    if (options.input.type == catcheye::input::InputSourceType::Camera &&
        options.input.camera_pipeline.empty() &&
        options.input.camera_device.empty()) {
        options.input.camera_pipeline = std::string(DEFAULT_CAMERA_PIPELINE);
        options.input.camera_width = DEFAULT_CAMERA_WIDTH;
        options.input.camera_height = DEFAULT_CAMERA_HEIGHT;
    }

    if (options.viewer_only) {
        if (options.input.type != catcheye::input::InputSourceType::Camera) {
            throw std::runtime_error("--viewer-only is only valid with --camera");
        }
        if (options.publisher_type == PublisherType::None) {
            throw std::runtime_error("--viewer-only requires --ws");
        }
        if (!options.hef_path.empty() || !options.metadata_path.empty() || !options.positional_args.empty()) {
            throw std::runtime_error("model, metadata, and ROI path arguments are not used with --viewer-only");
        }
        if (!options.pallet_roi_config_path.empty()) {
            throw std::runtime_error("pallet ROI path is not used with --viewer-only");
        }
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
        .metadata_path = resolve_default_model_path(executable_path, "metadata.yaml"),
        .hef_path = resolve_default_model_path(executable_path, "yolo26m.hef"),
        .roi_config_path = resolve_default_config_path(executable_path, "roi_cam_default.json"),
        .pallet_roi_config_path = resolve_default_config_path(executable_path, "pallet_roi_cam_default.json"),
        .camera_properties_path = resolve_default_config_path(executable_path, "camera_properties.json"),
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
        .loaded = true,
        .path = roi_config_path,
        .config = roi_parse_result.config,
    };
}

AppBootstrap build_app_bootstrap(
    const AppOptions& options,
    const DefaultPaths& default_paths,
    const LoadedRoiConfig& loaded_roi_config,
    const LoadedRoiConfig& loaded_pallet_roi_config) {
    AppBootstrap bootstrap;

    bootstrap.processor_config.detection_enabled = !options.viewer_only;
    bootstrap.processor_config.detector.backend = catcheye::DetectorBackend::Hailo;

    auto& hailo_cfg = bootstrap.processor_config.detector.hailo;
    hailo_cfg.hef_path = options.hef_path.empty() ? default_paths.hef_path : options.hef_path;
    hailo_cfg.metadata_path = options.metadata_path.empty() ? default_paths.metadata_path : options.metadata_path;
    hailo_cfg.allowed_class_ids = {0, options.pallet_class_id};

    // ── ROI / runtime 설정 ──────────────────────────────────
    bootstrap.processor_config.roi_enabled = loaded_roi_config.loaded;
    bootstrap.processor_config.roi_config_path = loaded_roi_config.path;
    bootstrap.processor_config.roi_config = loaded_roi_config.config;
    bootstrap.processor_config.pallet_detection_enabled = !options.viewer_only;
    bootstrap.processor_config.pallet_class_id = options.pallet_class_id;
    bootstrap.processor_config.pallet_roi_enabled = loaded_pallet_roi_config.loaded;
    bootstrap.processor_config.pallet_roi_config_path = loaded_pallet_roi_config.path;
    bootstrap.processor_config.pallet_roi_config = loaded_pallet_roi_config.config;
    bootstrap.processor_config.roi_alert_gpio.enabled = options.roi_alert_gpio >= 0;
    bootstrap.processor_config.roi_alert_gpio.chip_path = options.gpio_chip_path;
    bootstrap.processor_config.roi_alert_gpio.line = options.roi_alert_gpio;
    bootstrap.processor_config.roi_alert_gpio.active_low = options.roi_alert_active_low;
    bootstrap.processor_config.roi_alert_gpio.consumer = "catcheye-guard-roi-alert";
    bootstrap.processor_config.person_roi_alert_disable_gpio.enabled = options.person_roi_alert_disable_gpio >= 0;
    bootstrap.processor_config.person_roi_alert_disable_gpio.chip_path = options.gpio_chip_path;
    bootstrap.processor_config.person_roi_alert_disable_gpio.line = options.person_roi_alert_disable_gpio;
    bootstrap.processor_config.person_roi_alert_disable_gpio.active_low = options.person_roi_alert_disable_active_low;
    bootstrap.processor_config.person_roi_alert_disable_gpio.debounce_duration =
        std::chrono::milliseconds(options.person_roi_alert_disable_debounce_ms);
    bootstrap.processor_config.person_roi_alert_disable_gpio.consumer = "catcheye-guard-person-roi-alert-disable";

    bootstrap.runtime_config.process_every_n_frames = 2;
    bootstrap.publisher_type = options.publisher_type;
    bootstrap.websocket_publisher_config.port = options.websocket_port;
    bootstrap.http_api_server_config.port = options.http_port;

    if (!options.viewer_only && hailo_cfg.hef_path.empty()) {
        throw std::runtime_error("Hailo HEF path is required; pass --hef <model.hef>");
    }

    bootstrap.source = catcheye::input::create_frame_source(options.input);
    const std::string camera_properties_path =
        options.camera_properties_path == "config/camera_properties.json" ? default_paths.camera_properties_path : options.camera_properties_path;
    if (options.input.type == catcheye::input::InputSourceType::Camera) {
        CameraPropertyMap camera_properties = load_camera_properties_file(camera_properties_path);
        bootstrap.source = std::make_unique<CameraPropertiesFrameSource>(
            std::move(bootstrap.source),
            std::move(camera_properties),
            camera_properties_path);
    }
    return bootstrap;
}

int run_app(int argc, char** argv) {
    const AppOptions options = parse_app_options(argc, argv);
    if (options.show_help) {
        print_usage();
        return 0;
    }

    const DefaultPaths default_paths = resolve_default_paths(argv[0]);
    if (options.positional_args.size() > 1) {
        throw std::runtime_error("only one positional argument is supported: [roi.json]");
    }
    const std::string roi_config_path = !options.positional_args.empty() ? options.positional_args[0] : default_paths.roi_config_path;
    const std::string pallet_roi_config_path =
        options.pallet_roi_config_path.empty() ? default_paths.pallet_roi_config_path : options.pallet_roi_config_path;
    const std::string camera_properties_path =
        options.camera_properties_path == "config/camera_properties.json" ? default_paths.camera_properties_path : options.camera_properties_path;
    const LoadedRoiConfig loaded_roi_config = options.viewer_only ? LoadedRoiConfig{} : load_and_validate_roi_config(roi_config_path);
    const LoadedRoiConfig loaded_pallet_roi_config =
        options.viewer_only ? LoadedRoiConfig{} : load_and_validate_roi_config(pallet_roi_config_path);
    AppBootstrap bootstrap = build_app_bootstrap(options, default_paths, loaded_roi_config, loaded_pallet_roi_config);

    if (const auto log = logger()) {
        if (options.viewer_only) {
            log->info("catcheye-guard starting (mode='{}', publisher='{}')",
                      describe_runtime_mode(options),
                      publisher_name(bootstrap.publisher_type));
        } else {
            log->info("catcheye-guard starting (mode='{}', ROI='{}', pallet_ROI='{}', publisher='{}', http_port={})",
                      describe_runtime_mode(options),
                      bootstrap.processor_config.roi_config_path,
                      bootstrap.processor_config.pallet_roi_config_path,
                      publisher_name(bootstrap.publisher_type),
                      bootstrap.http_api_server_config.port);
        }
        if (bootstrap.processor_config.detection_enabled) {
            log->info("detector backend: hailo");
        } else {
            log->info("detector disabled: viewer-only mode");
        }
        if (bootstrap.processor_config.roi_alert_gpio.enabled) {
            log->info("ROI alert GPIO enabled: chip='{}', line={}, active_low={}",
                      bootstrap.processor_config.roi_alert_gpio.chip_path,
                      bootstrap.processor_config.roi_alert_gpio.line,
                      bootstrap.processor_config.roi_alert_gpio.active_low);
        }
        if (bootstrap.processor_config.person_roi_alert_disable_gpio.enabled) {
            log->info("person ROI alert disable GPIO enabled: chip='{}', line={}, active_low={}, debounce_ms={}",
                      bootstrap.processor_config.person_roi_alert_disable_gpio.chip_path,
                      bootstrap.processor_config.person_roi_alert_disable_gpio.line,
                      bootstrap.processor_config.person_roi_alert_disable_gpio.active_low,
                      bootstrap.processor_config.person_roi_alert_disable_gpio.debounce_duration.count());
        }
        log->info("RGB camera properties config: '{}'", camera_properties_path);
    }

    std::unique_ptr<catcheye::transport::ResultPublisher> publisher;
    if (bootstrap.publisher_type == PublisherType::WebSocket) {
        publisher = std::make_unique<catcheye::transport::WebSocketPublisher>(bootstrap.websocket_publisher_config);
    }
    auto recording_controller = std::make_unique<catcheye::RecordingController>(options.recording_dir);
    publisher = std::make_unique<catcheye::RecordingPublisher>(std::move(publisher), *recording_controller);

    const std::string http_roi_config_path = bootstrap.processor_config.roi_config_path;
    const std::string http_pallet_roi_config_path = bootstrap.processor_config.pallet_roi_config_path;
    auto processor = std::make_unique<catcheye::GuardProcessor>(std::move(bootstrap.processor_config));
    auto* processor_ptr = processor.get();
    auto* camera_source_ptr = bootstrap.source.get();
    auto http_api_server = std::make_unique<HttpApiServer>(
        bootstrap.http_api_server_config,
        http_roi_config_path,
        http_pallet_roi_config_path,
        processor_ptr,
        camera_source_ptr,
        recording_controller.get(),
        camera_properties_path);
    if (!http_api_server->start()) {
        throw std::runtime_error("failed to start HTTP API server");
    }

    catcheye::runtime::FrameProcessingRunner runner(std::move(bootstrap.runtime_config), std::move(bootstrap.source), std::move(processor),
                                                    std::move(publisher));
    const int exit_code = runner.run();
    if (http_api_server) {
        http_api_server->stop();
    }
    return exit_code;
}

} // namespace catcheye::guard
