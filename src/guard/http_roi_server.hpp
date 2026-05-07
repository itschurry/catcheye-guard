#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace catcheye {

class GuardProcessor;

struct HttpRoiServerConfig {
    std::string bind_address = "0.0.0.0";
    int port = 8090;
};

enum class RoiConfigKind {
    Person,
    Pallet,
};

class HttpRoiServer {
  public:
    HttpRoiServer(HttpRoiServerConfig config, std::string roi_config_path, std::string pallet_roi_config_path, GuardProcessor* processor);
    ~HttpRoiServer();

    bool start();
    void stop();

  private:
    void accept_loop();
    void handle_client(int client_fd);
    bool send_response(int client_fd, int status_code, const std::string& status_text, const std::string& body) const;
    bool handle_get_roi(int client_fd, RoiConfigKind kind);
    bool handle_put_roi(int client_fd, const std::string& body, RoiConfigKind kind);
    const std::string& roi_config_path(RoiConfigKind kind) const;

    HttpRoiServerConfig config_;
    std::string roi_config_path_;
    std::string pallet_roi_config_path_;
    GuardProcessor* processor_ = nullptr;
    int server_fd_ = -1;
    std::atomic<bool> running_ = false;
    std::thread accept_thread_;
};

} // namespace catcheye
