#pragma once
#include <string>
#include "severity.h"
#include "../time/clock.h"

namespace kb::log
{

struct LogEntry
{
    Severity severity = Severity::Info;
    const char* source_file = nullptr;
    const char* source_function = nullptr;
    int source_line = -1;
    TimeBase::TimeStamp timestamp;
    std::string message;
    // TODO: optional stack trace
};



}