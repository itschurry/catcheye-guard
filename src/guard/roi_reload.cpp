#include "guard/roi_reload.hpp"

#include <fstream>
#include <iterator>
#include <string>

#include "catcheye/guard/roi/roi_repository.hpp"
#include "catcheye/guard/roi/roi_validation.hpp"
#include "catcheye/utils/logger.hpp"

namespace catcheye {
namespace {

bool read_text_file(const std::string& path, std::string& contents, std::string& error_message) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        error_message = "failed to open file";
        return false;
    }

    contents.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    if (!ifs.good() && !ifs.eof()) {
        error_message = "failed while reading file";
        return false;
    }

    error_message.clear();
    return true;
}

} // namespace

bool try_reload_roi_config(
    GuardProcessorConfig& config,
    std::string& last_seen_roi_config_text,
    bool& roi_reload_watch_warning_emitted) {
    if (!config.roi_enabled || !config.roi_auto_reload || config.roi_config_path.empty()) {
        return false;
    }

    std::string current_text;
    std::string error_message;
    if (!read_text_file(config.roi_config_path, current_text, error_message)) {
        if (!roi_reload_watch_warning_emitted) {
            roi_reload_watch_warning_emitted = true;
            if (const auto log = logger()) {
                log->warn(
                    "failed to read ROI config '{}': {}",
                    config.roi_config_path,
                    error_message);
            }
        }
        return false;
    }

    roi_reload_watch_warning_emitted = false;
    if (current_text == last_seen_roi_config_text) {
        return false;
    }

    last_seen_roi_config_text = current_text;

    if (const auto log = logger()) {
        log->info("detected ROI config change, reloading '{}'", config.roi_config_path);
    }

    const auto parse_result = catcheye::guard::roi::RoiRepository::from_json_string(current_text);
    if (!parse_result.success) {
        if (const auto log = logger()) {
            log->warn("ROI reload failed, keeping previous config");
            for (const std::string& error : parse_result.errors) {
                log->warn("ROI parse error: {}", error);
            }
        }
        return false;
    }

    const auto validation_result = catcheye::guard::roi::validate_camera_roi_config(parse_result.config);
    if (!validation_result.valid) {
        if (const auto log = logger()) {
            log->warn("reloaded ROI config is invalid, keeping previous config");
            for (const auto& issue : validation_result.issues) {
                log->warn(
                    "ROI validation issue: zone_index={}, point_index={}, message={}",
                    issue.zone_index,
                    issue.point_index,
                    issue.message);
            }
        }
        return false;
    }

    config.roi_config = parse_result.config;
    if (const auto log = logger()) {
        log->info(
            "ROI config reloaded successfully: camera_id='{}', zones={}",
            config.roi_config.camera_id,
            config.roi_config.allowed_zones.size());
    }
    return true;
}

bool initialize_roi_reload_watch(
    const GuardProcessorConfig& config,
    std::string& last_seen_roi_config_text,
    bool& roi_reload_watch_warning_emitted) {
    if (!config.roi_enabled || !config.roi_auto_reload || config.roi_config_path.empty()) {
        return false;
    }

    std::string error_message;
    if (!read_text_file(config.roi_config_path, last_seen_roi_config_text, error_message)) {
        roi_reload_watch_warning_emitted = true;
        if (const auto log = logger()) {
            log->warn(
                "failed to initialize ROI file watch for '{}': {}",
                config.roi_config_path,
                error_message);
        }
        return false;
    }

    roi_reload_watch_warning_emitted = false;
    if (const auto log = logger()) {
        log->info("ROI auto-reload watching '{}'", config.roi_config_path);
    }
    return true;
}

} // namespace catcheye
