#pragma once
#include "../math/color.h"
#include "policy.h"
#include "severity.h"
#include "sink.h"
#include <memory>
#include <vector>

namespace kb::th
{
class JobSystem;
}

namespace kb::log
{

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

/**
 * @brief Decentralized message broker that directs all submitted log entries to the subscribed sinks
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
    Channel(Severity level, const std::string &full_name, const std::string &short_name, math::argb32_t tag_color);

    /**
     * @brief Add a sink to this channel
     *
     * Sinks can be shared by multiple channels
     *
     * @param psink
     */
    void attach_sink(std::shared_ptr<Sink> psink);

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

    inline const ChannelPresentation &get_presentation() const
    {
        return presentation_;
    }

    /**
     * @brief Transition the whole logging system to asynchronous mode
     *
     * In asynchronous mode, a given worker thread of the JobSystem will be in
     * charge of dispatching log entries to the sinks.
     *
     * @details Sinks will end up performing kernel calls to do their job. A kernel
     * call induces a kernel transition, which is slow, and tends to pollute the
     * CPU cache. Asynchronous mode will make the code around the call site
     * perform faster. Speed is only constrained by how fast we can push tasks
     * in a worker's queue, which is fast enough.
     *
     * @param js JobSystem instance. If set to nullptr, the logger will go back to synchronous mode.
     * @param worker thread ID of the worker that will get the logging tasks
     */
    static void set_async(th::JobSystem *js, uint32_t worker = 1);

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
     * @brief Intercept POSIX signals and force JobSystem to finish pending logging tasks before the program ends
     *
     * Asynchronous mode only.
     *
     * @details When the program crashes, we'd like to have a full log of what happened. When the program terminates,
     * pending logging tasks will be dropped. This setting allows set_async() to register signal handlers that will
     * be called when a signal is intercepted. They will force the JobSystem into "panic mode", during which all
     * workers are stopped and joined, and the caller thread will sequentially execute "essential" jobs in their
     * private queues. Logging tasks are marked essential, and are the only such tasks.
     *
     * @warning Highly experimental, certainly UB, may not work as intended.
     *
     * @param value
     */
    static inline void intercept_signals(bool value = true)
    {
        s_intercept_signals_ = value;
    }

    /**
     * @brief Dispatch log entries to the sinks
     *
     * Policies are executed first. If the entry passes the filter, it is propagated to the sinks.
     * - In synchronous mode, an entry is submitted to each sink sequentially on the caller thread.
     * Sink access is synchronized by a mutex.
     * - In asynchronous mode, sink dispatch is deferred to a worker thread. Task submission is lock-free.
     *
     * @param entry
     */
    void submit(struct LogEntry &&entry) const;

private:
    ChannelPresentation presentation_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    std::vector<std::shared_ptr<Policy>> policies_;
    Severity level_;

    static th::JobSystem *s_js_;
    static uint32_t s_worker_;
    static bool s_exit_on_fatal_error_;
    static bool s_intercept_signals_;
};

} // namespace kb::log