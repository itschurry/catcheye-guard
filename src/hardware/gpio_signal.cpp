#include "catcheye/hardware/gpio_signal.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

#include "catcheye/utils/logger.hpp"

#if CATCHEYE_HAS_LIBGPIOD
#include <gpiod.h>
#endif

namespace catcheye::hardware {
namespace {

std::chrono::milliseconds normalized_pulse_duration(std::chrono::milliseconds duration) {
    if (duration.count() < 0) {
        return std::chrono::milliseconds{0};
    }
    return duration;
}

} // namespace

class GpioPulseSignal::Backend {
  public:
    explicit Backend(GpioSignalConfig config)
        : config_(std::move(config)) {}

    bool initialize() {
#if CATCHEYE_HAS_LIBGPIOD
        chip_ = gpiod_chip_open(config_.chip_path.c_str());
        if (chip_ == nullptr) {
            log_errno("failed to open gpio chip");
            return false;
        }

        line_ = gpiod_chip_get_line(chip_, static_cast<unsigned int>(config_.line));
        if (line_ == nullptr) {
            log_errno("failed to get gpio line");
            close_chip();
            return false;
        }

        const int default_value = config_.active_low ? 1 : 0;
        const int request_result = gpiod_line_request_output(
            line_,
            config_.consumer.c_str(),
            default_value);
        if (request_result < 0) {
            log_errno("failed to request gpio line as output");
            close_line();
            close_chip();
            return false;
        }

        return true;
#else
        if (const auto log = logger()) {
            log->error(
                "libgpiod support is unavailable. This build was configured without libgpiod headers.");
        }
        return false;
#endif
    }

    void set_active(bool active) {
#if CATCHEYE_HAS_LIBGPIOD
        if (line_ == nullptr) {
            return;
        }

        const int value = config_.active_low ? (active ? 0 : 1) : (active ? 1 : 0);
        if (gpiod_line_set_value(line_, value) < 0) {
            log_errno("failed to set gpio value");
        }
#else
        (void)active;
#endif
    }

    ~Backend() {
#if CATCHEYE_HAS_LIBGPIOD
        close_line();
        close_chip();
#endif
    }

  private:
    void log_errno(const std::string& message) const {
        if (const auto log = logger()) {
            log->error("{}: {} (chip='{}', line={})", message, std::strerror(errno), config_.chip_path, config_.line);
        }
    }

#if CATCHEYE_HAS_LIBGPIOD
    void close_line() {
        if (line_ != nullptr) {
            gpiod_line_release(line_);
            line_ = nullptr;
        }
    }

    void close_chip() {
        if (chip_ != nullptr) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
        }
    }

    gpiod_chip* chip_ = nullptr;
    gpiod_line* line_ = nullptr;
#endif
    GpioSignalConfig config_;
};

GpioPulseSignal::GpioPulseSignal(GpioSignalConfig config)
    : config_(std::move(config)) {}

GpioPulseSignal::~GpioPulseSignal() {
    shutdown();
}

bool GpioPulseSignal::initialize() {
    if (!config_.enabled || config_.line < 0) {
        return true;
    }

    backend_ = std::make_unique<Backend>(config_);
    if (!backend_->initialize()) {
        backend_.reset();
        return false;
    }

    worker_ = std::thread(&GpioPulseSignal::worker_loop, this);
    initialized_ = true;

    if (const auto log = logger()) {
        log->info("GPIO pulse signal ready: chip='{}', line={}, active_low={}, pulse_ms={}",
                  config_.chip_path,
                  config_.line,
                  config_.active_low,
                  normalized_pulse_duration(config_.pulse_duration).count());
    }
    return true;
}

void GpioPulseSignal::set_state(bool active) {
    if (!initialized_ || backend_ == nullptr) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        hold_state_ = active;
        hold_requested_ = true;
        pulse_requested_ = false;
        pulse_until_.reset();
    }
    cv_.notify_one();
}

void GpioPulseSignal::trigger() {
    if (!initialized_ || backend_ == nullptr) {
        return;
    }

    const auto duration = normalized_pulse_duration(config_.pulse_duration);
    if (duration.count() == 0) {
        set_active(true);
        set_active(false);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pulse_requested_ = true;
        const auto requested_until = std::chrono::steady_clock::now() + duration;
        pulse_until_ = std::max(pulse_until_.value_or(requested_until), requested_until);
    }
    cv_.notify_one();
}

void GpioPulseSignal::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ && worker_.joinable() == false) {
            backend_.reset();
            return;
        }
        stop_requested_ = true;
    }
    cv_.notify_one();

    if (worker_.joinable()) {
        worker_.join();
    }

    initialized_ = false;
    backend_.reset();
}

void GpioPulseSignal::worker_loop() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (!stop_requested_) {
        if (!pulse_requested_ && !pulse_until_.has_value() && !hold_requested_) {
            cv_.wait(lock, [this]() {
                return stop_requested_ || (pulse_requested_ && pulse_until_.has_value()) || hold_requested_;
            });
            if (stop_requested_) {
                break;
            }
        }

        if (hold_requested_) {
            const bool requested_state = hold_state_;
            hold_requested_ = false;
            lock.unlock();
            set_active(requested_state);
            lock.lock();
            continue;
        }

        const auto pulse_deadline = *pulse_until_;
        pulse_requested_ = false;
        lock.unlock();
        set_active(true);
        lock.lock();

        cv_.wait_until(lock, pulse_deadline, [this, pulse_deadline]() {
            return stop_requested_ || (pulse_until_.has_value() && *pulse_until_ > pulse_deadline);
        });
        if (stop_requested_) {
            break;
        }
        if (pulse_until_.has_value() && *pulse_until_ > pulse_deadline) {
            continue;
        }

        pulse_until_.reset();
        lock.unlock();
        set_active(false);
        lock.lock();
    }

    lock.unlock();
    set_active(false);
}

void GpioPulseSignal::set_active(bool active) {
    if (backend_ != nullptr) {
        backend_->set_active(active);
    }
}

} // namespace catcheye::hardware
