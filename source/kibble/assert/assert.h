#pragma once

#include "fmt/format.h"

#ifndef K_ASSERT_STACK_TRACE_SKIP
#define K_ASSERT_STACK_TRACE_SKIP 0
#endif

namespace detail
{
void k_assert_impl(const char* condition, std::string_view message, const char* file, int line, const char* function);
}

#define K_CHECK_IMPL(condition, message, ...)                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ::detail::k_assert_impl(#condition, fmt::format(message, __VA_ARGS__), __FILE__, __LINE__,                 \
                                    __PRETTY_FUNCTION__);                                                              \
        }                                                                                                              \
    } while (0)

#define K_CHECK(...) K_CHECK_IMPL(__VA_ARGS__, "")

#define K_FAIL(...) K_CHECK_IMPL(false, __VA_ARGS__, "")

#ifdef K_ENABLE_ASSERT
#define K_ASSERT(...) K_CHECK(__VA_ARGS__)
#else
#define K_ASSERT(...) ((void)0)
#endif