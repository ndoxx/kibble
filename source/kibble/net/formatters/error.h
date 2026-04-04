#include "kibble/net/error.h"

#include "fmt/core.h"

template <>
struct fmt::formatter<kb::net::NetError> : fmt::formatter<std::string>
{
    auto format(const kb::net::NetError& e, fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(e.message(), ctx);
    }
};

template <>
struct fmt::formatter<kb::net::WSError> : fmt::formatter<std::string>
{
    auto format(const kb::net::WSError& e, fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(e.message(), ctx);
    }
};

template <>
struct fmt::formatter<kb::net::WSWarning> : fmt::formatter<std::string>
{
    auto format(const kb::net::WSWarning& e, fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(e.message(), ctx);
    }
};