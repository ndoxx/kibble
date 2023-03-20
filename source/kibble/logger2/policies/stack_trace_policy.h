#pragma once

#include "../policy.h"
#include "../severity.h"

namespace kb::log
{

class StackTracePolicy : public Policy
{
public:
    StackTracePolicy(Severity level, size_t skip = 0);

    bool transform_filter(struct LogEntry &entry) const override;

private:
    Severity level_;
    size_t skip_;
};

}