#include "stack_trace.h"
#include "backward-cpp/backward.hpp"
#include <sstream>

namespace kb
{

namespace internal_deleter
{
template <>
void Deleter<backward::StackTrace>::operator()(backward::StackTrace *p)
{
    delete p;
}
} // namespace internal_deleter

StackTrace::StackTrace(size_t skip): skip_(skip)
{
    // [[maybe_unused]] backward::TraceResolver unused__; // see https://github.com/bombela/backward-cpp/issues/206
    ptrace_ = make_internal<backward::StackTrace>();
    ptrace_->load_here(64);
    ptrace_->skip_n_firsts(skip_);
}

StackTrace::StackTrace(const StackTrace &other) : ptrace_(make_internal<backward::StackTrace>(*other.ptrace_))
{
}

StackTrace &StackTrace::operator=(const StackTrace &other)
{
    ptrace_ = make_internal<backward::StackTrace>(*other.ptrace_);
    return *this;
}

std::string StackTrace::format() const
{
    std::ostringstream oss;
    backward::Printer printer;
    // printer.object = true;
    // printer.color_mode = backward::ColorMode::always;
    // printer.address = true;
    // printer.snippet = true;
    printer.print(*ptrace_, oss);

    return oss.str();
}

} // namespace kb