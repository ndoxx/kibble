#include "kibble/assert/assert.h"
#include "kibble/platform/platform.h"
#include "kibble/util/stack_trace.h"

#include "fmt/color.h"
#include <stdexcept>

#if defined(K_PLATFORM_LINUX)
#include "kibble/util/debug_break.h"
#define debug_break debug_break__
#elif defined(K_PLATFORM_WINDOWS)
#if defined(K_COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#endif
#include <windows.h>
#if defined(K_COMPILER_CLANG)
#pragma clang diagnostic pop
#endif
#define debug_break DebugBreak
#endif

namespace detail
{

void k_assert_impl(const char* condition, std::string_view message, const char* file, int line, const char* function)
{
    fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nAssertion failed: {}\n  -> {}\n  -> in {} at {}:{}\n{}\n",
               condition, message, function, file, line, kb::StackTrace(K_ASSERT_STACK_TRACE_SKIP).format());

    debug_break();
}

void k_assert_except_impl(const char* condition, std::string_view message, const char* file, int line,
                          const char* function)
{
    throw std::runtime_error(
        fmt::format("Assertion failed: {}\n  -> {}\n  -> in {} at {}:{}", condition, message, function, file, line)
            .c_str());
}

} // namespace detail