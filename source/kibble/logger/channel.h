#pragma once
#include "kibble/logger/policy.h"
#include "kibble/logger/severity.h"
#include "kibble/logger/sink.h"
#include "kibble/math/color.h"

#include <memory>
#include <mutex>
#include <vector>

namespace kb::log
{

struct LogEntry;
class Channel;

/**
 * @brief Textual and visual information about a channel that can be used by formatters for styling
 *
 */
struct ChannelPresentation
{
    std::string full_name;
    std::string tag;
    math::argb32_t color;
};

namespace detail
{
void dispatch_entry(const Channel& channel, const LogEntry& entry);
}

/**
 * @brief Decentralized message broker that directs all submitted log entries to the subscribed sinks
 *
 * Submission is always asynchronous: a single dedicated worker thread owned by the logging system
 * pops entries off a lock-free MPSC queue and dispatches them to the sinks. Filtering (severity check,
 * policies) happens on the calling thread; only sink I/O is deferred to the worker.
 *
 */
class Channel
{
public:
    /**
     * @brief Construct a new Channel
     *
     * @param level severity threshold at which the log will be propagated to the sinks
     * @param full_name Full name of this channel
     * @param short_name Short name of this channel, used by terminal formatters
     * @param tag_color The color used by relevant formatters to display this channel
     */
    Channel(Severity level, const std::string& full_name, const std::string& short_name, math::argb32_t tag_color);

    /**
     * @brief Block until every entry submitted so far has been dispatched
     *
     * This is required so that no queued entry can ever reference a destroyed channel: the
     * worker thread only ever touches sinks_/presentation_ through a raw pointer captured at
     * submit() time.
     *
     */
    ~Channel();

    /// @brief Call when no more channel is alive to kill the worker thread
    static void shutdown();

    /**
     * @brief Add a sink to this channel
     *
     * Sinks can be shared by multiple channels
     *
     * @param psink
     */
    void attach_sink(std::shared_ptr<Sink> psink);

    /**
     * @brief Remove a sink from this channel
     *
     * @param psink
     */
    void detach_sink(const std::shared_ptr<Sink>& psink);

    /**
     * @brief Add a policy to this channel
     *
     * Policies allow to transform and filter log entries
     *
     * @param ppolicy
     */
    void attach_policy(std::shared_ptr<Policy> ppolicy);

    /**
     * @brief Change the severity threshold dynamically
     *
     * @param level
     */
    inline void set_severity_level(Severity level)
    {
        level_ = level;
    }

    inline const ChannelPresentation& get_presentation() const
    {
        return presentation_;
    }

    /**
     * @brief Configure logging system to exit after a log entry with Fatal severity is dispatched
     *
     * @param value
     */
    static inline void exit_on_fatal_error(bool value = true)
    {
        s_exit_on_fatal_error_ = value;
    }

    /**
     * @brief Intercept POSIX signals so the logging worker gets a chance to drain pending entries
     * before the program ends
     *
     * @details Registers handlers for SIGABRT, SIGFPE, SIGILL, SIGINT, SIGSEGV and SIGTERM on first
     * call with value set to true. The handler only flips an atomic flag; the worker thread notices it
     * and drains whatever is left in the queue before stopping.
     *
     * @warning Highly experimental, certainly UB, may not work as intended.
     *
     * @param value
     */
    static void intercept_signals(bool value = true);

    /**
     * @brief Enqueue a log entry for dispatch to the sinks
     *
     * Policies run synchronously on the calling thread. If the entry passes every policy, it is
     * pushed onto the lock-free queue for the worker thread to hand off to the sinks.
     *
     * @param entry
     */
    void submit(struct LogEntry&& entry) const;

    /**
     * @brief Block until every entry submitted so far has been dispatched, then flush the sinks
     *
     */
    void flush() const;

private:
    ChannelPresentation presentation_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    std::vector<std::shared_ptr<Policy>> policies_;
    Severity level_;
    mutable std::mutex sink_mutex_;

    static bool s_exit_on_fatal_error_;
    static bool s_intercept_signals_;

    friend void detail::dispatch_entry(const Channel&, const LogEntry&);
};

} // namespace kb::log