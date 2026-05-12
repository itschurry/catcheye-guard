#include "guard/http_roi_server.hpp"

#include <utility>

#include "catcheye/http/roi_api.hpp"
#include "catcheye/utils/logger.hpp"
#include "guard/processor.hpp"

namespace catcheye {

HttpRoiServer::HttpRoiServer(HttpRoiServerConfig config, std::string roi_config_path, std::string pallet_roi_config_path, GuardProcessor* processor)
    : config_(std::move(config)),
      roi_config_path_(std::move(roi_config_path)),
      pallet_roi_config_path_(std::move(pallet_roi_config_path)),
      processor_(processor) {}

HttpRoiServer::~HttpRoiServer()
{
    stop();
}

bool HttpRoiServer::start()
{
    if (server_ != nullptr) {
        return true;
    }
    if (processor_ == nullptr || config_.port <= 0) {
        return false;
    }

    server_ = std::make_unique<catcheye::http::HttpServer>(catcheye::http::HttpServerConfig{
        .bind_address = config_.bind_address,
        .port = config_.port,
    });
    catcheye::http::register_roi_routes(
        *server_,
        catcheye::http::RoiApiConfig{
            .person_roi_path = roi_config_path_,
            .pallet_roi_path = pallet_roi_config_path_,
            .apply = [this](catcheye::http::RoiConfigKind kind, const catcheye::roi::CameraRoiConfig& roi_config) {
                return kind == catcheye::http::RoiConfigKind::Pallet
                    ? processor_->update_pallet_roi_config(roi_config)
                    : processor_->update_roi_config(roi_config);
            },
        });

    if (!server_->start()) {
        server_.reset();
        return false;
    }

    if (const auto log = logger()) {
        log->info("ROI HTTP API listening on {}:{}", config_.bind_address, config_.port);
    }
    return true;
}

void HttpRoiServer::stop()
{
    if (server_ != nullptr) {
        server_->stop();
        server_.reset();
    }
}

} // namespace catcheye
