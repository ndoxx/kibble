#pragma once

namespace kb::log
{

class Policy
{
public:
    virtual ~Policy() = default;
    virtual bool transform_filter(struct LogEntry &entry) const = 0;
};

} // namespace kb::log