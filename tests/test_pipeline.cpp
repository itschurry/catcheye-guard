#include "test_support.hpp"

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

#include "catcheye/input/frame_source.hpp"
#include "catcheye/core/pipeline.hpp"

namespace fs = std::filesystem;

namespace {

class FakeFrameSource final : public catcheye::input::FrameSource {
   public:
    explicit FakeFrameSource(std::vector<catcheye::input::FrameReadStatus> statuses)
        : statuses_(std::move(statuses)) {}

    bool open() override
    {
        opened_ = true;
        return true;
    }

    bool is_open() const override
    {
        return opened_;
    }

    catcheye::input::FrameReadStatus read(catcheye::input::Frame& frame) override
    {
        if (!opened_ || index_ >= statuses_.size()) {
            return catcheye::input::FrameReadStatus::EndOfStream;
        }

        const catcheye::input::FrameReadStatus status = statuses_[index_++];
        if (status == catcheye::input::FrameReadStatus::Ok) {
            frame.image = cv::Mat(32, 32, CV_8UC3, cv::Scalar(0, 0, 0)).clone();
            frame.format = catcheye::input::PixelFormat::BGR;
            frame.timestamp = 1;
        }
        return status;
    }

    void close() override
    {
        opened_ = false;
    }

    std::string describe() const override
    {
        return "fake";
    }

   private:
    std::vector<catcheye::input::FrameReadStatus> statuses_;
    std::size_t index_ = 0;
    bool opened_ = false;
};

catcheye::PipelineConfig make_pipeline_config()
{
    const fs::path root = fs::path(CATCHEYE_SOURCE_DIR);

    catcheye::PipelineConfig config;
    config.render_preview = false;
    config.stream_preview = false;
    config.roi_enabled = false;
    config.detector.input_width = 640;
    config.detector.input_height = 640;
    config.detector.param_path = (root / "models" / "yolo26n_ncnn_model" / "model.ncnn.param").string();
    config.detector.bin_path = (root / "models" / "yolo26n_ncnn_model" / "model.ncnn.bin").string();
    config.detector.metadata_path = (root / "models" / "yolo26n_ncnn_model" / "metadata.yaml").string();
    return config;
}

} // namespace

TEST_CASE(pipeline_returns_zero_after_processing_frames_before_end_of_stream)
{
    auto source = std::make_unique<FakeFrameSource>(
        std::vector<catcheye::input::FrameReadStatus> {
            catcheye::input::FrameReadStatus::Ok,
            catcheye::input::FrameReadStatus::EndOfStream,
        });

    catcheye::Pipeline pipeline(make_pipeline_config(), std::move(source));
    test_support::assert_true(pipeline.run() == 0, "pipeline should succeed after processing one frame");
}

TEST_CASE(pipeline_returns_one_on_read_error)
{
    auto source = std::make_unique<FakeFrameSource>(
        std::vector<catcheye::input::FrameReadStatus> {
            catcheye::input::FrameReadStatus::Error,
        });

    catcheye::Pipeline pipeline(make_pipeline_config(), std::move(source));
    test_support::assert_true(pipeline.run() == 1, "pipeline should fail on read error");
}

TEST_CASE(pipeline_returns_one_when_source_ends_before_first_frame)
{
    auto source = std::make_unique<FakeFrameSource>(
        std::vector<catcheye::input::FrameReadStatus> {
            catcheye::input::FrameReadStatus::EndOfStream,
        });

    catcheye::Pipeline pipeline(make_pipeline_config(), std::move(source));
    test_support::assert_true(pipeline.run() == 1, "pipeline should fail when source is empty");
}
