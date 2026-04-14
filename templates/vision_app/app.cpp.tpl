#include "__APP_NS__/__APP_NS___app.hpp"

#include <span>
#include <stdexcept>
#include <string>

#include "catcheye/runtime/frame_processing_runner.hpp"
#include "catcheye/transport/result_publisher.hpp"
#include "catcheye/transport/websocket_publisher.hpp"
#include "catcheye/utils/logger.hpp"
#include "__APP_NS__/__APP_NS___processor.hpp"

namespace catcheye::__APP_NS__ {
namespace {

bool is_input_mode(const std::string& arg)
{
    return arg == "--image" || arg == "--video" || arg == "--camera";
}

} // namespace

AppOptions parse_app_options(int argc, char** argv)
{
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
        } else if (arg == "--ws") {
            options.publish_results = true;
            options.render_preview = false;
            if (i + 1 < args.size() && args[i + 1][0] != '-') {
                ++i;
                options.websocket_port = std::stoi(args[i]);
            }
        } else if (arg == "--ws-with-preview") {
            options.publish_results = true;
            options.render_preview = true;
        } else if (arg == "--headless") {
            options.render_preview = false;
        } else if (arg == "--image" || arg == "--video" || arg == "--camera-pipeline") {
            throw std::runtime_error("missing value for flag: " + arg);
        } else if (is_input_mode(arg)) {
            throw std::runtime_error("input mode flags are mutually exclusive");
        } else {
            options.positional_args.push_back(arg);
        }
    }

    if ((options.input.type == catcheye::input::InputSourceType::ImageFile
         || options.input.type == catcheye::input::InputSourceType::VideoFile)
        && !options.input.camera_pipeline.empty()) {
        throw std::runtime_error("--camera-pipeline is only valid with --camera");
    }

    if ((options.input.type == catcheye::input::InputSourceType::ImageFile
         || options.input.type == catcheye::input::InputSourceType::VideoFile)
        && options.input.uri.empty()) {
        throw std::runtime_error("input path is required for --image or --video");
    }

    return options;
}

AppBootstrap build_app_bootstrap(const AppOptions& options)
{
    AppBootstrap bootstrap;
    bootstrap.processor_config.stream_name = "__APP_SLUG__";
    bootstrap.runtime_config.window_name = "__APP_TITLE__";
    bootstrap.runtime_config.render_preview = options.render_preview;
    bootstrap.runtime_config.process_every_n_frames = 1;
    bootstrap.publish_results = options.publish_results;
    bootstrap.publisher_config.port = options.websocket_port;
    bootstrap.source = catcheye::input::create_frame_source(options.input);
    return bootstrap;
}

int run_app(int argc, char** argv)
{
    const AppOptions options = parse_app_options(argc, argv);
    AppBootstrap bootstrap = build_app_bootstrap(options);

    if (const auto log = logger()) {
        log->info(
            "__APP_SLUG__ starting (preview={}, ws={})",
            bootstrap.runtime_config.render_preview,
            bootstrap.publish_results);
    }

    std::unique_ptr<catcheye::transport::ResultPublisher> publisher;
    if (bootstrap.publish_results) {
        publisher = std::make_unique<catcheye::transport::WebSocketPublisher>(bootstrap.publisher_config);
    }

    auto processor = std::make_unique<__APP_CLASS__Processor>(std::move(bootstrap.processor_config));
    catcheye::runtime::FrameProcessingRunner runner(
        std::move(bootstrap.runtime_config),
        std::move(bootstrap.source),
        std::move(processor),
        std::move(publisher));
    return runner.run();
}

} // namespace catcheye::__APP_NS__
