#include "file_sink.h"
#include "../channel.h"
#include "../entry.h"
#include "../severity.h"
#include <fmt/os.h>

namespace kb::log
{

struct FileSink::Internal
{
    Internal(const fs::path& filepath) : out{fmt::output_file(filepath.string())}
    {
    }

    fmt::ostream out;
};
} // namespace kb::log

namespace kb::internal_deleter
{
template <>
void Deleter<kb::log::FileSink::Internal>::operator()(kb::log::FileSink::Internal* p)
{
    delete p;
}
} // namespace kb::internal_deleter

namespace kb::log
{

FileSink::FileSink(const fs::path& filepath) : filepath_{filepath}
{
    internal_ = make_internal<Internal>(filepath_);
}

void FileSink::submit(const LogEntry& e, const struct ChannelPresentation& p)
{
    float ts = std::chrono::duration_cast<std::chrono::duration<float>>(e.timestamp).count();
    internal_->out.print("T{}:{:6.f} [{}] [{}] {}\n", e.thread_id, ts, p.full_name, to_str(e.severity), e.message);

    // print context info if needed
    if (uint8_t(e.severity) <= 2)
    {
        // clang-format off
        internal_->out.print("@ {}\n{}:{}\n", 
            e.source_location.function_name, 
            e.source_location.file_name,
            e.source_location.line
        );
        // clang-format on
    }

    // print stack trace
    if (e.stack_trace.has_value())
    {
        internal_->out.print("{}", e.stack_trace->format());
    }
}

void FileSink::flush() const
{
    internal_->out.flush();
}

} // namespace kb::log