#pragma once

namespace kb::log
{

class Policy
{
public:
    virtual ~Policy() = default;
    virtual bool filter(const struct LogEntry &entry) const = 0;
};

} // namespace kb::log