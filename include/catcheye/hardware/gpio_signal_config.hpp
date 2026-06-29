#pragma once

#include <string>

namespace catcheye {

struct GpioSignalConfig {
    bool enabled = false;
    std::string chip_path = "/dev/gpiochip4";
    int line = -1;
    bool active_low = false;
    std::string consumer = "catcheye-guard";
};

} // namespace catcheye
