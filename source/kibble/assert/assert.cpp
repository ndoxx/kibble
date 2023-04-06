#include "assert.h"
#include "../logger2/entry_builder.h"

#include <iostream>
#include <stdexcept>

namespace kb::util
{

Assertion::Assertion(std::string expression, const kb::log::Channel *channel, const char *file, const char *function,
                     int line, Trigger trigger)
    : channel_{channel}, file_{file}, function_{function}, line_{line}, trigger_{trigger}
{
    stream_ << "Assertion failed: " << expression << '\n';
}

Assertion::~Assertion()
{
    if (channel_)
    {
        kb::log::EntryBuilder{channel_, line_, file_, function_}
            .level(trigger_ == Trigger::Terminate ? log::Severity::Fatal : log::Severity::Error)
            .log(stream_.str());
    }
    else
    {
        std::cerr << stream_.str() << std::endl;
        if (trigger_ == Trigger::Terminate)
            std::terminate();
    }
}

void Assertion::ex()
{
    trigger_ = Trigger::Exception;
    throw std::runtime_error(stream_.str());
}

} // namespace kb::util