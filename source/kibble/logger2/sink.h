#pragma once
#include <memory>
#include <mutex>
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
    virtual void on_attach(){};
    virtual void on_detach(){};
    virtual void submit(const LogEntry &, const ChannelPresentation &) = 0;
    inline void set_formatter(std::shared_ptr<Formatter> formatter)
    {
        formatter_ = formatter;
    }

    inline void submit_lock(const LogEntry &e, const ChannelPresentation &p)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        submit(e, p);
    }

protected:
    std::shared_ptr<Formatter> formatter_ = nullptr;

private:
    std::mutex mutex_;
};

} // namespace kb::log