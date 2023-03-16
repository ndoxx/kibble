#pragma once

#include "../../net/tcp_stream.h"
#include "../sink.h"
#include <functional>
#include <string>

namespace kb::log
{

class NetSink : public Sink
{
public:
    using StreamVisitor = std::function<void(net::TCPStream &)>;

    ~NetSink();
    void submit(const struct LogEntry &, const struct ChannelPresentation &) override;
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

    inline void set_on_attach_callback(StreamVisitor v)
    {
        on_attach_ = v;
    }

    inline void set_on_destroy_callback(StreamVisitor v)
    {
        on_destroy_ = v;
    }

private:
    uint16_t port_;
    std::string server_;
    net::TCPStream *stream_ = nullptr;
    StreamVisitor on_attach_ = [](net::TCPStream &) {};
    StreamVisitor on_destroy_ = [](net::TCPStream &) {};
};

} // namespace kb::log