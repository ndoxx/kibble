/**
 * @file assert.h
 * @brief Better assertions.
 *
 * The macros in this file make assertions more useful. A source file and code line will be printed when an assertion
 * fails, and the user will be prompted with a choice to either debug-break, print the backtrace, continue anyway, or
 * exit.\n
 * All the macros are optimized out in the retail build.
 *
 * @author ndx
 * @version 0.1
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021
 *
 */

#pragma once

/**
 * @def K_DEBUG_BREAK()
 * Break into debugger.
 *
 * @def K_ASSERT(CND, STR)
 * Assert that a condition CND is true. If it is not, print an error message STR with some contextual information and
 * prompt the user with a choice to either debug-break, print the backtrace, continue anyway, or exit.
 *
 * @def K_ASSERT_FMT(CND, FORMAT_STR, ...)
 * Like K_ASSERT(CND, STR) but with a printf-like API. FORMAT_STR is a formatted string, and all the following arguments
 * are evaluated and will replace the placeholders in FORMAT_STR.
 *
 * @def K_STATIC_ERROR(FSTR, ...)
 * Only calls printf, forwarding it the formatted string FSTR and the rest of the arguments.
 *
 */

// Macro to break into debugger
#ifdef K_DEBUG
#ifdef __linux__
#include <csignal>
#define K_DEBUG_BREAK() raise(SIGTRAP)
#endif
#ifdef _WIN32
#define K_DEBUG_BREAK() __debugbreak()
#endif
#else
#define K_DEBUG_BREAK()
#endif

#ifdef K_ENABLE_ASSERT
#include <cstdio>
namespace detail
{
extern void assert_redirect();
}
#define ASSERT_FMT_BUFFER_SIZE__ 128
static char ASSERT_FMT_BUFFER__[ASSERT_FMT_BUFFER_SIZE__];
#define K_STATIC_ERROR(FSTR, ...) printf(FSTR, __VA_ARGS__)
#define K_ASSERT_FMT(CND, FORMAT_STR, ...)                                                                             \
    {                                                                                                                  \
        if (!(CND))                                                                                                    \
        {                                                                                                              \
            snprintf(ASSERT_FMT_BUFFER__, ASSERT_FMT_BUFFER_SIZE__, FORMAT_STR, __VA_ARGS__);                          \
            printf("\033[1;38;2;255;0;0mAssertion failed:\033[0m '%s' -- %s\n%s:%s:%d\n", #CND, ASSERT_FMT_BUFFER__,   \
                   __FILE__, __func__, __LINE__);                                                                      \
            ::detail::assert_redirect();                                                                               \
        }                                                                                                              \
    }
#define K_ASSERT(CND, STR)                                                                                             \
    {                                                                                                                  \
        if (!(CND))                                                                                                    \
        {                                                                                                              \
            printf("\033[1;38;2;255;0;0mAssertion failed:\033[0m '%s' -- %s\n%s:%s:%d\n", #CND, STR, __FILE__,         \
                   __func__, __LINE__);                                                                                \
            ::detail::assert_redirect();                                                                               \
        }                                                                                                              \
    }
#else
#define K_STATIC_ERROR(FSTR, ...)
#define K_ASSERT_FMT(CND, FORMAT_STR, ...)
#define K_ASSERT(CND, STR)
#endif