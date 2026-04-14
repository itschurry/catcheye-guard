#include "__APP_NS__/__APP_NS___processor.hpp"

#include <sstream>
#include <utility>

namespace catcheye::__APP_NS__ {
namespace {

std::string build_metadata_json(const catcheye::runtime::ProcessContext& context)
{
    std::ostringstream oss;
    oss << "{"
        << "\"app\":\"__APP_SLUG__\","
        << "\"frame_index\":" << context.frame_index
        << "}";
    return oss.str();
}

} // namespace

__APP_CLASS__Processor::__APP_CLASS__Processor(__APP_CLASS__ProcessorConfig config)
    : config_(std::move(config))
{
}

bool __APP_CLASS__Processor::initialize()
{
    return true;
}

catcheye::runtime::ProcessOutput __APP_CLASS__Processor::process(
    const catcheye::input::Frame& frame,
    const catcheye::runtime::ProcessContext& context)
{
    catcheye::runtime::ProcessOutput output;
    if (!context.needs_preview && !context.needs_publish) {
        return output;
    }

    cv::Mat preview = frame.image.clone();

    // TODO:
    // 여기서 앱 알고리즘 돌리고 overlay / metadata를 채우면 된다.

    if (context.needs_preview) {
        output.has_preview = true;
        output.preview_frame = preview.clone();
    }

    if (context.needs_publish) {
        output.has_message = true;
        output.message = catcheye::protocol::encode_jpeg_frame(
            preview,
            build_metadata_json(context),
            config_.stream_name,
            config_.jpeg_quality);
    }

    return output;
}

} // namespace catcheye::__APP_NS__
