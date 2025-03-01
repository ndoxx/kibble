#include "kibble/util/stack_trace.h"

// #define BACKWARD_HAS_BFD 1
#include "backward-cpp/backward.hpp"
#include <sstream>

namespace kb
{

StackTrace::StackTrace(size_t skip) : skip_(skip)
{
    // [[maybe_unused]] backward::TraceResolver unused__; // see https://github.com/bombela/backward-cpp/issues/206
    ptrace_ = std::make_unique<backward::StackTrace>();
    ptrace_->load_here(64);
    ptrace_->skip_n_firsts(skip_);
}

StackTrace::StackTrace(const StackTrace& other) : ptrace_(std::make_unique<backward::StackTrace>(*other.ptrace_))
{
}

StackTrace::~StackTrace()
{
    // For PIMPL
}

StackTrace& StackTrace::operator=(const StackTrace& other)
{
    ptrace_ = std::make_unique<backward::StackTrace>(*other.ptrace_);
    return *this;
}

std::string StackTrace::format() const
{
    std::ostringstream oss;
    backward::Printer printer;
    printer.object = true;
    printer.color_mode = backward::ColorMode::always;
    printer.address = true;
    printer.snippet = true;
    printer.inliner_context_size = 5;
    printer.trace_context_size = 7;
    printer.reverse = true;
    printer.print(*ptrace_, oss);

    return oss.str();
}

} // namespace kb