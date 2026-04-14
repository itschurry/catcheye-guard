#pragma once

#include "catcheye/protocol/frame_message.hpp"
#include "catcheye/runtime/frame_processor.hpp"
#include "__APP_NS__/__APP_NS___processor_config.hpp"

namespace catcheye::__APP_NS__ {

class __APP_CLASS__Processor final : public catcheye::runtime::FrameProcessor {
public:
    explicit __APP_CLASS__Processor(__APP_CLASS__ProcessorConfig config);

    bool initialize() override;
    catcheye::runtime::ProcessOutput process(
        const catcheye::input::Frame& frame,
        const catcheye::runtime::ProcessContext& context) override;

private:
    __APP_CLASS__ProcessorConfig config_;
};

} // namespace catcheye::__APP_NS__
