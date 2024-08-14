#include "kibble/logger/sinks/console_sink.h"
#include "kibble/logger/formatter.h"

namespace kb::log
{
void ConsoleSink::submit(const LogEntry& e, const struct ChannelPresentation& p)
{
    formatter_->print(e, p);
}

} // namespace kb::log