#pragma once
#include "../time/clock.h"
#include "severity.h"
#include "util/stack_trace.h"
#include <optional>
#include <string>

namespace kb::log
{

struct SourceLocation
{
    const char *file = nullptr;
    const char *function = nullptr;
    int line = -1;
};

struct LogEntry
{
    Severity severity = Severity::Info;
    SourceLocation source_location;
    TimeBase::TimeStamp timestamp;
    std::string message; // TODO: use string_view, string should be managed
    std::optional<StackTrace> stack_trace = {};
};

} // namespace kb::log