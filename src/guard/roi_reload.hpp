#pragma once

#include <string>

#include "guard/processor_config.hpp"

namespace catcheye {

bool try_reload_roi_config(
    GuardProcessorConfig& config,
    std::string& last_seen_roi_config_text,
    bool& roi_reload_watch_warning_emitted);

bool initialize_roi_reload_watch(
    const GuardProcessorConfig& config,
    std::string& last_seen_roi_config_text,
    bool& roi_reload_watch_warning_emitted);

} // namespace catcheye
