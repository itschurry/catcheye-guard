#pragma once

#include <memory>
#include <vector>

#include "catcheye/input/frame_source.hpp"
#include "catcheye/runtime/frame_processing_runner.hpp"
#include "catcheye/transport/websocket_publisher.hpp"
#include "__APP_NS__/__APP_NS___processor_config.hpp"

namespace catcheye::__APP_NS__ {

struct AppOptions {
    catcheye::input::InputSourceConfig input;
    bool publish_results = false;
    bool render_preview = true;
    int websocket_port = 8080;
    std::vector<std::string> positional_args;
};

struct AppBootstrap {
    __APP_CLASS__ProcessorConfig processor_config;
    catcheye::runtime::RuntimeConfig runtime_config;
    catcheye::transport::WebSocketPublisherConfig publisher_config;
    bool publish_results = false;
    std::unique_ptr<catcheye::input::FrameSource> source;
};

AppOptions parse_app_options(int argc, char** argv);
AppBootstrap build_app_bootstrap(const AppOptions& options);
int run_app(int argc, char** argv);

} // namespace catcheye::__APP_NS__
