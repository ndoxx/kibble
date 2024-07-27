#include "assert/assert.h"
#include "util/debug_break.h"
#include "util/stack_trace.h"

#include "fmt/color.h"

namespace detail
{

void k_assert_impl(const char* condition, std::string_view message, const char* file, int line, const char* function)
{
    fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "\nAssertion failed: {}\n  -> {}\n  -> in {} at {}:{}\n{}\n",
               condition, message, function, file, line, kb::StackTrace(K_ASSERT_STACK_TRACE_SKIP).format());

    debug_break__();
}

} // namespace detail