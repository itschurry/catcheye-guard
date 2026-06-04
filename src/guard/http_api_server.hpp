#pragma once

#include <memory>
#include <string>

#include "catcheye/http/http_server.hpp"
#include "catcheye/input/frame_source.hpp"

namespace catcheye {

class GuardProcessor;
class RecordingController;

struct HttpApiServerConfig {
    std::string bind_address = "0.0.0.0";
    int port = 8090;
};

class HttpApiServer {
  public:
    HttpApiServer(
        HttpApiServerConfig config,
        std::string roi_config_path,
        std::string pallet_roi_config_path,
        GuardProcessor* processor,
        catcheye::input::FrameSource* camera_source,
        RecordingController* recording_controller);
    ~HttpApiServer();

    bool start();
    void stop();

  private:
    HttpApiServerConfig config_;
    std::string roi_config_path_;
    std::string pallet_roi_config_path_;
    GuardProcessor* processor_ = nullptr;
    catcheye::input::FrameSource* camera_source_ = nullptr;
    RecordingController* recording_controller_ = nullptr;
    std::unique_ptr<catcheye::http::HttpServer> server_;
};

} // namespace catcheye
