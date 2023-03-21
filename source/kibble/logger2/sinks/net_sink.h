#pragma once

#include "../../net/tcp_stream.h"
#include "../sink.h"
#include <functional>
#include <string>

namespace kb::log
{

/**
 * @brief Direct all input log entries to a TCP socket
 * 
 * The formatter will decide how exactly the logs are structured
 * 
 */
class NetSink : public Sink
{
public:
    using AttachCallback = std::function<void(net::TCPStream &, const Channel&)>;
    using DestroyCallback = std::function<void(net::TCPStream &)>;

    ~NetSink();

    void submit(const LogEntry &, const ChannelPresentation &) override;

    void on_attach(const Channel&) override;

    /**
     * @brief Connect to a remote machine.
     *
     * @param server IP address of the server
     * @param port TCP port to use
     * @return true if the connection was successful
     * @return false otherwise
     */
    bool connect(const std::string &server, uint16_t port);

    /**
     * @brief Setup a callback to be called when the sink is attached to a channel
     * 
     * @param v 
     */
    inline void set_on_attach_callback(AttachCallback v)
    {
        on_attach_ = v;
    }

    /**
     * @brief Setup a callback to be called when the sink is destroyed
     * 
     * @param v 
     */
    inline void set_on_destroy_callback(DestroyCallback v)
    {
        on_destroy_ = v;
    }

private:
    uint16_t port_;
    std::string server_;
    net::TCPStream *stream_ = nullptr;
    AttachCallback on_attach_ = [](net::TCPStream &, const Channel&) {};
    DestroyCallback on_destroy_ = [](net::TCPStream &) {};
};

} // namespace kb::log