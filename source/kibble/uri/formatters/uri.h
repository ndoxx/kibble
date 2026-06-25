/**
 * @file uri_fmt.h
 * @brief fmtlib formatters for kb::uri types, for logging and CLI tools.
 *
 * Kept out of uri.h on purpose: the parser itself has no reason to depend on
 * fmt, this header is opt-in for whoever wants to print the IR.
 */
#pragma once

#include "kibble/uri/uri.h"

#include "fmt/format.h"
#include "fmt/ranges.h"

namespace kb::uri
{

/// @brief Human readable name for a UriParseError value.
[[nodiscard]] constexpr std::string_view to_string_view(UriParseError error) noexcept
{
    switch (error)
    {
    case UriParseError::EmptyURI:
        return "EmptyURI";
    case UriParseError::MissingSchemeSeparator:
        return "MissingSchemeSeparator";
    case UriParseError::EmptyScheme:
        return "EmptyScheme";
    case UriParseError::EmptyPath:
        return "EmptyPath";
    }
    return "Unknown";
}

} // namespace kb::uri

template <>
struct fmt::formatter<kb::uri::UriParseError> : fmt::formatter<std::string_view>
{
    auto format(kb::uri::UriParseError error, format_context& ctx) const
    {
        return fmt::formatter<std::string_view>::format(kb::uri::to_string_view(error), ctx);
    }
};

template <>
struct fmt::formatter<kb::uri::KeyValueView>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const kb::uri::KeyValueView& kv, format_context& ctx) const
    {
        if (kv.value.empty())
        {
            return fmt::format_to(ctx.out(), "{}", kv.key);
        }
        return fmt::format_to(ctx.out(), "{}={}", kv.key, kv.value);
    }
};

template <>
struct fmt::formatter<kb::uri::URIView>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const kb::uri::URIView& uri, format_context& ctx) const
    {
        auto out = fmt::format_to(ctx.out(), "{}://{}", uri.scheme, uri.path);

        if (uri.has_query())
        {
            out = fmt::format_to(out, "?{}", fmt::join(uri.query, "&"));
        }
        if (uri.has_fragment())
        {
            out = fmt::format_to(out, "#{}", fmt::join(uri.fragment, "&"));
        }
        return out;
    }
};

template <>
struct fmt::formatter<kb::uri::UriParseResult>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const kb::uri::UriParseResult& result, format_context& ctx) const
    {
        if (result.has_value())
        {
            return fmt::format_to(ctx.out(), "{}", result.value());
        }
        return fmt::format_to(ctx.out(), "<parse error: {}>", result.error());
    }
};