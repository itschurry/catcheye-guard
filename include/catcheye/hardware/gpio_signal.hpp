#pragma once

#include <memory>
#include <string>

#include "catcheye/hardware/gpio_signal_config.hpp"

namespace catcheye::hardware {

class GpioStateSignal {
  public:
    explicit GpioStateSignal(GpioSignalConfig config);
    ~GpioStateSignal();

    GpioStateSignal(const GpioStateSignal&) = delete;
    GpioStateSignal& operator=(const GpioStateSignal&) = delete;

    bool initialize();
    void set_state(bool active);
    void shutdown();

  private:
    class Backend;

    void set_active(bool active);

    GpioSignalConfig config_;
    std::unique_ptr<Backend> backend_;
    bool initialized_ = false;
};

} // namespace catcheye::hardware
