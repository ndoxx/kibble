#pragma once
#include <memory>
#include <mutex>
#include <string>

namespace kb::log
{

struct LogEntry;
struct ChannelPresentation;
class Formatter;
class Channel;

/**
 * @brief Interface for log entry consumers.
 *
 * A sink can be specialized to send log entry to a standard output, a TCP socket,
 * anywhere else.
 *
 */
class Sink
{
public:
    virtual ~Sink() = default;

    /**
     * @brief Executed when the sink is attached to a channel
     *
     */
    virtual void on_attach(const Channel &){};

    /**
     * @brief Treat the log entry
     *
     */
    virtual void submit(const LogEntry &, const ChannelPresentation &) = 0;

    /**
     * @brief Mutex-synchronize submission
     *
     * @param e
     * @param p
     */
    inline void submit_lock(const LogEntry &e, const ChannelPresentation &p)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        submit(e, p);
    }

    /**
     * @brief Set the formatter used by this sink
     *
     * @param formatter
     */
    inline void set_formatter(std::shared_ptr<Formatter> formatter)
    {
        formatter_ = formatter;
    }

protected:
    std::shared_ptr<Formatter> formatter_ = nullptr;

private:
    std::mutex mutex_;
};

} // namespace kb::log