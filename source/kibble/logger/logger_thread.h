#pragma once

#include <map>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "logger_common.h"

namespace kb
{
namespace log
{

class Sink;
class LoggerThread
{
public:
    LoggerThread();
    ~LoggerThread();

    // Create a logging channel to group information of the same kind
    void create_channel(const std::string& name, uint8_t verbosity=3);
    // Attach a sink to a list of channels
	void attach(const std::string& sink_name, std::unique_ptr<Sink> sink, const std::vector<hash_t>& channels);
    // Attach a sink to all channels
	void attach_all(const std::string& sink_name, std::unique_ptr<Sink> sink);
    // Launch logger thread
    void spawn();
    // Wait for logger thread to be in idle state
    void sync();
    // Stop thread execution (waits for pending statements to be processed)
    void kill();
    // Push a single log statement into the queue
    void enqueue(LogStatement&& stmt);
	void enqueue(const LogStatement& stmt);
    // Ddispatch log statements to registered sinks
    void flush();

    // Get channel verbosity by name
    inline uint32_t get_channel_verbosity(hash_t name) const
    {
        return channels_.at(name).verbosity;
    }
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
    // Enable/Disable single threaded mode
    inline void set_single_threaded(bool value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        single_threaded_ = value;
    }
	// Enable/Disable a registered tracker
	void set_sink_enabled(hash_t name, bool value);

protected:
    enum State: int
    {
        STATE_IDLE,	 // The thread does nothing and is ready to queue more statements
        STATE_FLUSH, // The queue is emptied and the statements dispatched to sinks
        STATE_KILLED // The thread must halt properly and will join
    };

    // Called before the thread enters its main loop
    void thread_init();
    // Serve queuing requests (state machine impl)
    void thread_run();
    // Called before the thread joins
    void thread_cleanup();
    // Send a log statement to each sink (depending on their channel subscriptions)
    void dispatch(const LogStatement& stmt);

private:
    std::vector<LogStatement> log_statements_; // Stores log statements, access is synced
    std::mutex mutex_;						   // To lock/unlock access to the queue

    std::condition_variable cv_consume_; // Allows to wait passively for a state change
    std::condition_variable cv_update_;  // Allows to notify producer threads that the logger thread is ready to accept new statements
    std::thread logger_thread_;			 // Thread handle
    std::atomic<int> thread_state_;		 // Contains the state of this thread's state machine

	bool backtrace_on_error_; // Enable/Disable automatic backtrace submission on severe statements
    bool single_threaded_;    // If true, logger dispatches statements directly on submit

	std::map<hash_t, std::unique_ptr<Sink>> sinks_;		     // Registered sinks by hash name
	std::map<hash_t, LogChannel> channels_;				     // Registered channels by hash name
	std::multimap<hash_t, size_t> sink_subscriptions_;	     // Sinks can subscribe to multiple channels
};

#if LOGGING_ENABLED==1
struct Logger
{
	static std::unique_ptr<LoggerThread> LOGGER_THREAD;
};
#endif

} // namespace log

#if LOGGING_ENABLED==1
    #define WLOGGER( INSTR ) (*log::Logger::LOGGER_THREAD).INSTR;
#else
    #define WLOGGER( INSTR )
#endif
} // namespace kb