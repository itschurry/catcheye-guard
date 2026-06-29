#include "catcheye/hardware/gpio_signal.hpp"

#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

#include "catcheye/utils/logger.hpp"

#include <gpiod.h>

namespace catcheye::hardware {
class GpioStateSignal::Backend {
  public:
    explicit Backend(GpioSignalConfig config)
        : config_(std::move(config)) {}

    bool initialize() {
        chip_ = gpiod_chip_open(config_.chip_path.c_str());
        if (chip_ == nullptr) {
            log_errno("failed to open gpio chip");
            return false;
        }

        offset_ = static_cast<unsigned int>(config_.line);

        gpiod_request_config* request_config = gpiod_request_config_new();
        if (request_config == nullptr) {
            log_errno("failed to create gpio request config");
            close_chip();
            return false;
        }
        gpiod_request_config_set_consumer(request_config, config_.consumer.c_str());

        gpiod_line_settings* settings = gpiod_line_settings_new();
        if (settings == nullptr) {
            log_errno("failed to create gpio line settings");
            gpiod_request_config_free(request_config);
            close_chip();
            return false;
        }

        if (gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT) < 0) {
            log_errno("failed to configure gpio line as output");
            gpiod_line_settings_free(settings);
            gpiod_request_config_free(request_config);
            close_chip();
            return false;
        }

        gpiod_line_settings_set_active_low(settings, config_.active_low);
        if (gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE) < 0) {
            log_errno("failed to configure initial gpio value");
            gpiod_line_settings_free(settings);
            gpiod_request_config_free(request_config);
            close_chip();
            return false;
        }

        gpiod_line_config* line_config = gpiod_line_config_new();
        if (line_config == nullptr) {
            log_errno("failed to create gpio line config");
            gpiod_line_settings_free(settings);
            gpiod_request_config_free(request_config);
            close_chip();
            return false;
        }

        if (gpiod_line_config_add_line_settings(line_config, &offset_, 1, settings) < 0) {
            log_errno("failed to add gpio line settings");
            gpiod_line_config_free(line_config);
            gpiod_line_settings_free(settings);
            gpiod_request_config_free(request_config);
            close_chip();
            return false;
        }

        request_ = gpiod_chip_request_lines(chip_, request_config, line_config);

        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(settings);
        gpiod_request_config_free(request_config);

        if (request_ == nullptr) {
            log_errno("failed to request gpio line as output");
            close_chip();
            return false;
        }

        return true;
    }

    void set_active(bool active) {
        if (request_ == nullptr) {
            return;
        }

        const auto value = active ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
        if (gpiod_line_request_set_value(request_, offset_, value) < 0) {
            log_errno("failed to set gpio value");
        }
    }

    ~Backend() {
        close_line();
        close_chip();
    }

  private:
    void log_errno(const std::string& message) const {
        if (const auto log = logger()) {
            log->error("{}: {} (chip='{}', line={})", message, std::strerror(errno), config_.chip_path, config_.line);
        }
    }

    void close_line() {
        if (request_ != nullptr) {
            gpiod_line_request_release(request_);
            request_ = nullptr;
        }
    }

    void close_chip() {
        if (chip_ != nullptr) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
        }
    }

    gpiod_chip* chip_ = nullptr;
    gpiod_line_request* request_ = nullptr;
    unsigned int offset_ = 0;
    GpioSignalConfig config_;
};

GpioStateSignal::GpioStateSignal(GpioSignalConfig config)
    : config_(std::move(config)) {}

GpioStateSignal::~GpioStateSignal() {
    shutdown();
}

bool GpioStateSignal::initialize() {
    if (!config_.enabled || config_.line < 0) {
        return true;
    }

    backend_ = std::make_unique<Backend>(config_);
    if (!backend_->initialize()) {
        backend_.reset();
        return false;
    }

    initialized_ = true;

    if (const auto log = logger()) {
        log->info("GPIO state signal ready: chip='{}', line={}, active_low={}",
                  config_.chip_path,
                  config_.line,
                  config_.active_low);
    }
    return true;
}

void GpioStateSignal::set_state(bool active) {
    if (!initialized_ || backend_ == nullptr) {
        return;
    }

    set_active(active);
}

void GpioStateSignal::shutdown() {
    set_active(false);
    initialized_ = false;
    backend_.reset();
}

void GpioStateSignal::set_active(bool active) {
    if (backend_ != nullptr) {
        backend_->set_active(active);
    }
}

} // namespace catcheye::hardware
