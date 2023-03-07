#pragma once
#include <memory>
#include <string>

namespace kb::log
{

struct LogEntry;
struct ChannelPresentation;
class Formatter;
class Sink
{
public:
    virtual ~Sink() = default;
    virtual void submit(const LogEntry &, const ChannelPresentation &) = 0;
    virtual void on_attach(){};
    virtual void on_detach(){};

    inline void set_formatter(std::shared_ptr<Formatter> formatter)
    {
        formatter_ = formatter;
    }

protected:
    std::shared_ptr<Formatter> formatter_ = nullptr;
};

} // namespace kb::log