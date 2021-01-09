#pragma once

#include <fstream>
#include <string>
#include <vector>
#include "../hash/hash.h"

namespace kb
{
namespace net
{
class TCPStream;
}

namespace klog
{

struct LogStatement;
struct LogChannel;

// Base class for logger sinks.
// A sink can be registered by the logger thread and will be fed
// the log statements that have been queued up each time the queue is flushed.
class Sink
{
public:
    struct ChannelDescription
    {
        hash_t id;
        std::string name;
    };

    virtual ~Sink() = default;

    // Submit a log statement to this sink, specifying the channel it emanates from
    virtual void send(const LogStatement& stmt, const LogChannel& chan) = 0;
    // Submit a raw string to this sink
    virtual void send_raw(const std::string& message) = 0;
    // Override this if some operations need to be performed before logger destruction
    virtual void finish() {}
    // Called after channel subscription
    virtual void on_attach() {}

    inline void set_enabled(bool value) { enabled_ = value; }
    inline bool is_enabled() const { return enabled_; }

    inline void add_channel_subscription(const ChannelDescription& desc) { subscriptions_.push_back(desc); }
    inline const std::vector<ChannelDescription>& get_channel_subscriptions() const { return subscriptions_; }

protected:
    bool enabled_ = true;
    std::vector<ChannelDescription> subscriptions_;
};

// This sink writes to the terminal with ANSI color support
class ConsoleSink : public Sink
{
public:
    virtual ~ConsoleSink() = default;
    virtual void send(const LogStatement& stmt, const LogChannel& chan) override;
    virtual void send_raw(const std::string& message) override;
};

// This sink writes to a file (ANSI codes are stripped away)
class LogFileSink : public Sink
{
public:
    explicit LogFileSink(const std::string& filename);
    virtual ~LogFileSink() = default;
    virtual void send(const LogStatement& stmt, const LogChannel& chan) override;
    virtual void send_raw(const std::string& message) override;
    virtual void finish() override;

private:
    std::string filename_;
    std::ofstream out_;
};

// This sink writes to a TCP socket
class NetSink: public Sink
{
public:
    virtual ~NetSink();
    virtual void send(const LogStatement& stmt, const LogChannel& chan) override;
    virtual void send_raw(const std::string& message) override;
    virtual void on_attach() override;
    bool connect(const std::string& server, uint16_t port);

private:
    std::string server_;
    kb::net::TCPStream* stream_ = nullptr;
};

} // namespace klog
} // namespace kb