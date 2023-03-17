#include "net_sink.h"
#include "../formatter.h"
#include "net/tcp_connector.h"

namespace kb::log
{
NetSink::~NetSink()
{
    if (stream_)
    {
        // Notify server before closing connection
        on_destroy_(*stream_);
        delete stream_;
    }
}

void NetSink::submit(const LogEntry &e, const struct ChannelPresentation &p)
{
    stream_->send(formatter_->format_string(e, p));
}

void NetSink::on_attach()
{
    on_attach_(*stream_);
}

bool NetSink::connect(const std::string &server, uint16_t port)
{
    port_ = port;
    server_ = server;
    stream_ = net::TCPConnector::connect(server_, port_);
    return (stream_ != nullptr);
}

} // namespace kb::log