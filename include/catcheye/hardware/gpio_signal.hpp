#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "catcheye/hardware/gpio_signal_config.hpp"

namespace catcheye::hardware {

class GpioPulseSignal {
  public:
    explicit GpioPulseSignal(GpioSignalConfig config);
    ~GpioPulseSignal();

    GpioPulseSignal(const GpioPulseSignal&) = delete;
    GpioPulseSignal& operator=(const GpioPulseSignal&) = delete;

    bool initialize();
    void set_state(bool active);
    void trigger();
    void shutdown();

  private:
    class Backend;

    void worker_loop();
    void set_active(bool active);

    GpioSignalConfig config_;
    std::unique_ptr<Backend> backend_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool initialized_ = false;
    bool stop_requested_ = false;
    bool pulse_requested_ = false;
    bool hold_requested_ = false;
    bool hold_state_ = false;
    std::optional<std::chrono::steady_clock::time_point> pulse_until_;
};

} // namespace catcheye::hardware
