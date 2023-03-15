#pragma once
#include "../time/clock.h"
#include "severity.h"
#include "util/stack_trace.h"
#include <optional>
#include <string_view>

namespace kb::log
{

struct SourceLocation
{
    int line = -1;
    const char *file_name = nullptr;
    const char *function_name = nullptr;
};

struct LogEntry
{
    Severity severity = Severity::Info;
    SourceLocation source_location;
    TimeBase::TimeStamp timestamp;
    std::string_view message;
    std::optional<StackTrace> stack_trace = {};
};

} // namespace kb::log