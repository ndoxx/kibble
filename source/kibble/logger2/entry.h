#pragma once
#include "../time/clock.h"
#include "severity.h"
#include "util/stack_trace.h"
#include <optional>
#include <string>

namespace kb::log
{

struct LogEntry
{
    Severity severity = Severity::Info;
    const char *source_file = nullptr;
    const char *source_function = nullptr;
    int source_line = -1;
    TimeBase::TimeStamp timestamp;
    std::string message;
    std::optional<StackTrace> stack_trace = {};
};

} // namespace kb::log