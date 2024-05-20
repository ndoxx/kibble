#pragma once
#include <cstdint>
#include <string_view>

namespace kb::log
{

enum class Severity : int8_t
{
    Fatal = 0,
    Error,
    Warn,
    Info,
    Debug,
    Verbose
};

constexpr std::string_view to_str(Severity severity)
{
    // clang-format off
    switch(severity)
    {
        case Severity::Fatal:   return "Fatal";
        case Severity::Error:   return "Error";
        case Severity::Warn:    return "Warn";
        case Severity::Info:    return "Info";
        case Severity::Debug:   return "Debug";
        case Severity::Verbose: return "Verbose";
    }
    // clang-format on
}

} // namespace kb::log