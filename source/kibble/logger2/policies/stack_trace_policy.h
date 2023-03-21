#pragma once

#include "../policy.h"
#include "../severity.h"

namespace kb::log
{

class StackTracePolicy : public Policy
{
public:
    /**
     * @brief Setup policy to trigger a stack trace above given severity level
     *
     * @param level Severity threshold that triggers a stack trace
     * @param skip Depth to skip in the trace, avoids tracing logging related stuff
     */
    StackTracePolicy(Severity level, size_t skip = 0);

    ~StackTracePolicy() = default;

    /**
     * @brief Emplace a new stack trace in the log entry if severity leve is sufficient
     *
     * @param entry
     * @return true
     */
    bool transform_filter(struct LogEntry &entry) const override;

private:
    Severity level_;
    size_t skip_;
};

} // namespace kb::log