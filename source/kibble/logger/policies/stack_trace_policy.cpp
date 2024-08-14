#include "kibble/logger/policies/stack_trace_policy.h"
#include "kibble/logger/entry.h"

namespace kb::log
{

StackTracePolicy::StackTracePolicy(Severity level, size_t skip) : level_(level), skip_(skip)
{
}

bool StackTracePolicy::transform_filter(LogEntry& entry) const
{
    if (entry.severity <= level_)
    {
        entry.stack_trace.emplace(skip_);
    }

    return true;
}

} // namespace kb::log