#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "guard/guard_app.hpp"

namespace fs = std::filesystem;

TEST_CASE(parse_app_options_rejects_mutually_exclusive_input_modes)
{
    char arg0[] = "app";
    char arg1[] = "--image";
    char arg2[] = "a.png";
    char arg3[] = "--video";
    char arg4[] = "b.mp4";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4};

    bool threw = false;
    try {
        static_cast<void>(catcheye::guard::parse_app_options(5, argv));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    test_support::assert_true(threw, "expected parse_app_options to reject exclusive input modes");
}

TEST_CASE(parse_app_options_rejects_camera_pipeline_for_video_input)
{
    char arg0[] = "app";
    char arg1[] = "--video";
    char arg2[] = "input.mp4";
    char arg3[] = "--camera-pipeline";
    char arg4[] = "fake-pipeline";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4};

    bool threw = false;
    try {
        static_cast<void>(catcheye::guard::parse_app_options(5, argv));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    test_support::assert_true(threw, "expected parse_app_options to reject --camera-pipeline with --video");
}

TEST_CASE(build_app_bootstrap_applies_positional_overrides)
{
    catcheye::guard::AppOptions options;
    options.input.type = catcheye::input::InputSourceType::Camera;
    options.positional_args = {"param_override", "bin_override", "meta_override", "roi_override"};

    const catcheye::guard::DefaultPaths default_paths {
        .param_path = "param_default",
        .bin_path = "bin_default",
        .metadata_path = "meta_default",
        .roi_config_path = "roi_default",
    };
    const catcheye::guard::LoadedRoiConfig loaded_roi_config {
        .path = "roi_default",
        .config = {},
    };

    const catcheye::guard::AppBootstrap bootstrap =
        catcheye::guard::build_app_bootstrap(options, default_paths, loaded_roi_config);

    test_support::assert_true(
        bootstrap.processor_config.detector.param_path == "param_override",
        "param override mismatch");
    test_support::assert_true(
        bootstrap.processor_config.detector.bin_path == "bin_override",
        "bin override mismatch");
    test_support::assert_true(
        bootstrap.processor_config.detector.metadata_path == "meta_override",
        "metadata override mismatch");
    test_support::assert_true(bootstrap.processor_config.roi_config_path == "roi_override", "roi override mismatch");
}

TEST_CASE(parse_app_options_supports_headless_mode)
{
    char arg0[] = "app";
    char arg1[] = "--headless";
    char* argv[] = {arg0, arg1};

    const auto options = catcheye::guard::parse_app_options(2, argv);
    test_support::assert_true(!options.render_preview, "headless mode should disable preview");
}

TEST_CASE(parse_app_options_supports_websocket_mode)
{
    char arg0[] = "app";
    char arg1[] = "--ws";
    char arg2[] = "9001";
    char* argv[] = {arg0, arg1, arg2};

    const auto options = catcheye::guard::parse_app_options(3, argv);
    test_support::assert_true(options.publish_results, "ws mode should enable publisher");
    test_support::assert_true(!options.render_preview, "ws mode should disable preview by default");
    test_support::assert_true(options.websocket_port == 9001, "ws mode should parse port");
}

TEST_CASE(load_and_validate_roi_config_rejects_invalid_file)
{
    const fs::path bad_path = fs::temp_directory_path() / "catcheye_bad_roi_config.json";
    std::ofstream ofs(bad_path);
    ofs << R"json({"camera_id":123,"allowed_zones":[]})json";
    ofs.close();

    bool threw = false;
    try {
        static_cast<void>(catcheye::guard::load_and_validate_roi_config(bad_path.string()));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    fs::remove(bad_path);
    test_support::assert_true(threw, "expected invalid ROI config to throw");
}
