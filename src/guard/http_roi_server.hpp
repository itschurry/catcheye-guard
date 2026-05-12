#pragma once

#include <memory>
#include <string>

#include "catcheye/http/http_server.hpp"

namespace catcheye {

class GuardProcessor;

struct HttpRoiServerConfig {
    std::string bind_address = "0.0.0.0";
    int port = 8090;
};

class HttpRoiServer {
  public:
    HttpRoiServer(HttpRoiServerConfig config, std::string roi_config_path, std::string pallet_roi_config_path, GuardProcessor* processor);
    ~HttpRoiServer();

    bool start();
    void stop();

  private:
    HttpRoiServerConfig config_;
    std::string roi_config_path_;
    std::string pallet_roi_config_path_;
    GuardProcessor* processor_ = nullptr;
    std::unique_ptr<catcheye::http::HttpServer> server_;
};

} // namespace catcheye
