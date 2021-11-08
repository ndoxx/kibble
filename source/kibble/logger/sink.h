#pragma once

#include "../hash/hash.h"
#include <fstream>
#include <string>
#include <vector>

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

/**
 * @brief Base class for logger sinks.
 * A sink can be registered by the logger thread and will be fed the log statements that have been queued up each time
 * the queue is flushed.
 *
 */
class Sink
{
public:
    /**
     * @brief Describes a logging channel to be added as a subscription
     *
     */
    struct ChannelDescription
    {
        hash_t id;        /// Hashed name of the channel
        std::string name; /// Full name of the channel
    };

    virtual ~Sink() = default;

    /**
     * @brief Submit a log statement to this sink, specifying the channel it emanates from.
     *
     * @param stmt The log statement
     * @param chan The channel object that corresponds to stmt.channel
     */
    virtual void send(const LogStatement &stmt, const LogChannel &chan) = 0;

    /**
     * @brief Submit a raw string to this sink
     *
     * @param message
     */
    virtual void send_raw(const std::string &message) = 0;

    /**
     * @brief Override this if some operations need to be performed before logger destruction.
     *
     */
    virtual void finish()
    {
    }

    /**
     * @brief Called after channel subscription.
     *
     */
    virtual void on_attach()
    {
    }

    /**
     * @brief Enable or disable this sink.
     * A disabled sink will be ignored by the dispatcher.
     *
     * @param value true to enable the sink, false to disable it
     */
    inline void set_enabled(bool value)
    {
        enabled_ = value;
    }

    /**
     * @brief Check if this sink is enabled
     *
     * @return true if the sink is enabled
     * @return false otherwise
     */
    inline bool is_enabled() const
    {
        return enabled_;
    }

    /**
     * @brief Subscribe a channel to this sink
     *
     * @param desc Descriptor containing all the relevant info
     */
    inline void add_channel_subscription(const ChannelDescription &desc)
    {
        subscriptions_.push_back(desc);
    }

    /**
     * @brief Get the channel subscriptions list
     *
     * @return const std::vector<ChannelDescription>&
     */
    inline const std::vector<ChannelDescription> &get_channel_subscriptions() const
    {
        return subscriptions_;
    }

protected:
    bool enabled_ = true;
    std::vector<ChannelDescription> subscriptions_;
};

/**
 * @brief This sink writes to the terminal with ANSI color support
 *
 */
class ConsoleSink : public Sink
{
public:
    /**
     * @brief Print a statement to the console.
     * The timestamp will be displayed first, then the channel tag followed by a message type icon, then the message
     * content itself.
     *
     * @param stmt The logging statement to display
     * @param chan The channel this statement was sent to
     */
    void send(const LogStatement &stmt, const LogChannel &chan) override;

    /**
     * @brief Print a raw string
     *
     * @param message
     */
    void send_raw(const std::string &message) override;
};

/**
 * @brief This sink writes to a file (ANSI codes are stripped away).
 *
 */
class LogFileSink : public Sink
{
public:
    /**
     * @brief Construct a new Log File Sink, and initialize the path of its output file.
     *
     * @param filename Path to the output log file
     */
    explicit LogFileSink(const std::string &filename);

    /**
     * @brief Write a statement to the file.
     * The stream is flushed each time, so whenever the program fails and exits early, the log file is still usable.
     *
     * @param stmt The statement to write
     * @param chan The channel this statement was sent to
     */
    void send(const LogStatement &stmt, const LogChannel &chan) override;

    /**
     * @brief Write a raw string to the file
     *
     * @param message
     */
    void send_raw(const std::string &message) override;

    /**
     * @brief Tidily close the output file and notify the user that the file was saved.
     *
     */
    void finish() override;

private:
    std::string filename_;
    std::ofstream out_;
};

/**
 * @brief This sink writes to a TCP socket.
 *
 */
class NetSink : public Sink
{
public:
    /**
     * @brief Notify the remote object of the disconnection before deleting the stream.
     *
     */
    ~NetSink();

    /**
     * @brief Send a JSON-like stream containing all of the statement data to the remote object.
     *
     * The following JSON fields are set:
     * - "action": "msg"
     * - "channel": "<the channel name>"
     * - "type": "<32b message type>"
     * - "severity": "<32b severity>"
     * - "timestamp": "<timestamp>"
     * - "line": "<source line>"
     * - "file": "<source file>"
     * - "message": "<base64 encoded message>"
     *
     * @param stmt The statement to send
     * @param chan The channel this statement was sent to
     */
    void send(const LogStatement &stmt, const LogChannel &chan) override;

    /**
     * @brief Send a string through the socket as-is.
     *
     * @param message The raw string to send
     */
    void send_raw(const std::string &message) override;

    /**
     * @brief Updates the server with this sink's state.
     * First notify the remote object of the connection with a JSON stream.
     * The following JSON fields are set:
     * - "action": "connect"
     * - "peer_ip": "<the ip of the remote machine>"
     * - "peer_port": "<the port used for this connection>"
     *
     * Then send a description of the subscribed channels to the server.
     * The following JSON fields are set:
     * - "action": "set_channels"
     * - "channels": [<channel 1 name>, <channel 2 name>, ... <last channel name>]
     */
    void on_attach() override;

    /**
     * @brief Connect to a remote machine.
     *
     * @param server IP address of the server
     * @param port TCP port to use
     * @return true if the connection was successful
     * @return false otherwise
     */
    bool connect(const std::string &server, uint16_t port);

private:
    std::string server_;
    kb::net::TCPStream *stream_ = nullptr;
};

} // namespace klog
} // namespace kb