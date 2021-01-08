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
class LogDispatcher
{
public:
    LogDispatcher();
    ~LogDispatcher();

    // Check if a logging channel exists
    void has_channel(hash_t hname, bool& result);
    // Create a logging channel to group information of the same kind
    void create_channel(const std::string& name, uint8_t verbosity = 3);
    // Attach a sink to a list of channels
    void attach(const std::string& sink_name, std::unique_ptr<Sink> sink, const std::vector<hash_t>& channels);
    // Attach a sink to all channels
    void attach_all(const std::string& sink_name, std::unique_ptr<Sink> sink);

    // Get channel verbosity by name
    inline uint32_t get_channel_verbosity(hash_t name) const { return channels_.at(name).verbosity; }
    // Change channel verbosity
    inline void set_channel_verbosity(hash_t name, uint32_t verbosity)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        channels_.at(name).verbosity = uint8_t(std::min(verbosity, 3u));
    }
    // Mute channel by setting its verbosity to 0
    inline void mute_channel(hash_t name)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        channels_.at(name).verbosity = 0;
    }
    // Enable/Disable backtrace printing on error message
    inline void set_backtrace_on_error(bool value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        backtrace_on_error_ = value;
    }
    // Enable/Disable a registered tracker
    void set_sink_enabled(hash_t name, bool value);

    // Send a log statement to each sink (depending on their channel subscriptions)
    void dispatch(const LogStatement& stmt);

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