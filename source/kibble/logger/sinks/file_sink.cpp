#include "kibble/logger/sinks/file_sink.h"
#include "kibble/logger/channel.h"
#include "kibble/logger/entry.h"
#include "kibble/logger/severity.h"

namespace kb::log
{

FileSink::FileSink(const fs::path& filepath) : filepath_{filepath}, out_(fmt::output_file(filepath.string()))
{
}

FileSink::~FileSink()
{
}

void FileSink::submit(const LogEntry& e, const ChannelPresentation& p)
{
    float ts = std::chrono::duration_cast<std::chrono::duration<float>>(e.timestamp).count();
    out_.print(fmt::runtime("T{}:{:6.f} [{}] [{}] {}\n"), e.thread_id, ts, p.full_name, to_str(e.severity), e.message);

    // print context info if needed
    if (uint8_t(e.severity) <= 2)
    {
        // clang-format off
        out_.print("@ {}\n{}:{}\n", 
            e.source_location.function_name, 
            e.source_location.file_name,
            e.source_location.line
        );
        // clang-format on
    }

    // print stack trace
    if (e.stack_trace.has_value())
    {
        out_.print("{}", e.stack_trace->format());
    }
}

void FileSink::flush() const
{
    out_.flush();
}

} // namespace kb::log