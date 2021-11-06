#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <thread>

#include "common.h"

namespace kb
{
namespace klog
{

class Sink;

/**
 * @brief The dispatcher is the central hub responsible for distributing the logging messages to the relevant sinks
 * that consume them. This object should only be accessed by the part of the program responsible for logging
 * configuration. Once the dispatcher has been set up, only the LoggerStream objects need be accessed, thanks to the
 * KLOGx macros.
 *
 * Mutative calls to this object are mutex-synced. This in itself makes the logger rather slow, compared to other
 * loggers that work in a lock free fashion, but truth be told, I did not care that much about the performances to begin
 * with.
 *
 * Only one global instance of this object should exist in a program. To make things easier, the instance is stored as a
 * static shared pointer inside the Logger struct, and should only be accessed through macro calls:
 * - KLOGGER_START() will create the dispatcher instance
 * - KLOGGER() allows indirect access to the dispatcher's methods. Creating a channel, for instance, should look like:
 * `KLOGGER(create_channel("channel_name",3))`
 * - KLOGGER_SHARE_INSTANCE() allows to handle the edge case where some logger code exists in a linked library, as only
 * one dispatcher instance is allowed to exist
 *
 * @note All the previously mentioned macros are effective only when the symbol *LOGGING_ENABLED* is defined and set
 * to 1.
 */
class LogDispatcher
{
public:
    /**
     * @brief Construct a new Log Dispatcher object, and create a channel named "core" with default verbosity level 3.
     * In practice, the KLOGGER_START() macro is used to create a global dispatcher instance, and the constructor should
     * never be called by hand.
     *
     */
    LogDispatcher();

    /**
     * @brief Call each sink's Sink::finish() function and destroy this dispatcher.
     * In practice, the global logger instance is a static shared pointer and is automatically destroyed when the
     * program exits, so there is no need to worry about the destruction of this object
     *
     */
    ~LogDispatcher();

    /**
     * @brief Check if a logging channel exists
     *
     * @param hname Hashed name of the putative channel
     * @param result
     */
    void has_channel(hash_t hname, bool &result);

    /**
     * @brief Create a logging channel to group information of the same kind
     * If a channel already exists at that name, the creation will abort and the function will do nothing.
     * The styled tag used to display this channel is automatically generated. The tag color is taken from a fixed
     * palette, and the tag's short name is the first three characters of the channel name.
     *
     * @param name Name of the channel
     * @param verbosity Initial verbosity level of this channel
     */
    void create_channel(const std::string &name, uint8_t verbosity = 3);

    /**
     * @brief Override a channel's tag style
     *
     * @param name Target channel
     * @param custom_short_name Custom short name to be used in the tag, only the first three characters are taken into
     * account.
     * @param color Custom color to use in the tag
     */
    void set_channel_tag(const std::string &name, const std::string &custom_short_name, math::argb32_t color);

    /**
     * @brief Attach a sink to a list of channels.
     * The sink will receive all the messages sent to the channels that are attached to it.
     * The dispatcher will take full ownership of th sink.
     *
     * @param sink_name Full name of the sink
     * @param sink Pointer to the sink
     * @param channels List of hashed channel names
     * @see attach_all()
     */
    void attach(const std::string &sink_name, std::unique_ptr<Sink> sink, const std::vector<hash_t> &channels);

    /**
     * @brief Attach a sink to all currently existing channels.
     *
     * @param sink_name
     * @param sink
     * @see attach()
     */
    void attach_all(const std::string &sink_name, std::unique_ptr<Sink> sink);

    /**
     * @brief Get channel verbosity by name
     *
     * @param name Hashed name of the channel
     * @return uint32_t
     */
    inline uint32_t get_channel_verbosity(hash_t name) const
    {
        return channels_.at(name).verbosity;
    }

    /**
     * @brief Change channel verbosity by name
     *
     * @param name Hashed name of the channel
     * @param verbosity New verbosity level
     */
    inline void set_channel_verbosity(hash_t name, uint32_t verbosity)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        channels_.at(name).verbosity = uint8_t(std::min(verbosity, 3u));
    }

    /**
     * @brief Mute channel.
     * This is short hand for setting the verbosity level to zero.
     *
     * @param name Hashed name of the channel
     */
    inline void mute_channel(hash_t name)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        channels_.at(name).verbosity = 0;
    }

    /**
     * @brief Enable/Disable backtrace printing on error message.
     *
     * @param value if set to true, any message with a severity strictly greater than 1 will trigger a stack trace that
     * will be sent as raw text to all the relevant sinks.
     */
    inline void set_backtrace_on_error(bool value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        backtrace_on_error_ = value;
    }

    /**
     * @brief Enable/Disable a registered sink.
     * A disabled sink will not respond to any logging statement and will be merely ignored during dispatch.
     *
     * @param name Name of the sink
     * @param value if true, enables the sink, if false, disables it.
     */
    void set_sink_enabled(hash_t name, bool value);

    /**
     * @brief Send a log statement to each sink (depending on their channel subscriptions)
     *
     * @param stmt The statement to dispatch
     */
    void dispatch(const LogStatement &stmt);

protected:
    enum State : int
    {
        STATE_IDLE,  // The thread does nothing and is ready to queue more statements
        STATE_FLUSH, // The queue is emptied and the statements dispatched to sinks
        STATE_KILLED // The thread must halt properly and will join
    };

private:
    std::mutex mutex_;        // To lock/unlock access to the sinks
    bool backtrace_on_error_; // Enable/Disable automatic backtrace submission on severe statements
    std::map<hash_t, std::unique_ptr<Sink>> sinks_;    // Registered sinks by hash name
    std::map<hash_t, LogChannel> channels_;            // Registered channels by hash name
    std::multimap<hash_t, size_t> sink_subscriptions_; // Sinks can subscribe to multiple channels
};

/**
 * @brief Struct to hold the global instance of the dispatcher as a static member
 */
struct Logger
{
    static std::shared_ptr<LogDispatcher> DISPATCHER;
};

} // namespace klog

#if LOGGING_ENABLED == 1
#define KLOGGER_START() kb::klog::Logger::DISPATCHER = std::make_shared<kb::klog::LogDispatcher>()
#define KLOGGER_SHARE_INSTANCE(INSTANCE) kb::klog::Logger::DISPATCHER = INSTANCE // TODO: Also share time base
#define KLOGGER(INSTR) (*kb::klog::Logger::DISPATCHER).INSTR;
#else
#define KLOGGER_START()
#define KLOGGER_SHARE_INSTANCE(INSTANCE)
#define KLOGGER(INSTR)
#endif
} // namespace kb