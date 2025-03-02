#include "kibble/util/stack_trace.h"

#include "cpptrace/cpptrace.hpp"
#include <sstream>

namespace kb
{

StackTrace::StackTrace(size_t skip) : skip_(skip)
{
    ptrace_ = std::make_unique<cpptrace::raw_trace>(cpptrace::generate_raw_trace(skip_));
}

StackTrace::StackTrace(const StackTrace& other) : ptrace_(std::make_unique<cpptrace::raw_trace>(*other.ptrace_))
{
}

StackTrace::~StackTrace()
{
    // For PIMPL
}

StackTrace& StackTrace::operator=(const StackTrace& other)
{
    // Handle self-assignment problem
    if (this != &other)
    {
        ptrace_ = std::make_unique<cpptrace::raw_trace>(*other.ptrace_);
    }
    return *this;
}

std::string StackTrace::format(bool color) const
{
    return ptrace_->resolve().to_string(color);
}

} // namespace kb