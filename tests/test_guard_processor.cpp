#include "test_support.hpp"

#include "guard/guard_processor.hpp"

TEST_CASE(guard_processor_rejects_invalid_roi_config_before_detector_init)
{
    catcheye::GuardProcessorConfig config;
    config.detector.param_path = "unused";
    config.detector.bin_path = "unused";
    config.roi_enabled = true;
    config.roi_config.camera_id = "cam0";
    config.roi_config.image_width = 0;
    config.roi_config.image_height = 0;

    catcheye::GuardProcessor processor(config);
    test_support::assert_true(!processor.initialize(), "guard processor should reject invalid ROI config");
}
