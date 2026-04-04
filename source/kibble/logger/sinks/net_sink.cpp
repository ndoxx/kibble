#include "kibble/logger/sinks/net_sink.h"
#include "kibble/logger/formatter.h"
#include "kibble/net/tcp_connector.h"

#if defined(K_PLATFORM_LINUX)

namespace kb::log
{
NetSink::~NetSink()
{
    if (stream_)
    {
        // Notify server before closing connection
        on_destroy_(*stream_);
        delete stream_.release();
    }
}

void NetSink::submit(const LogEntry& e, const ChannelPresentation& p)
{
    (void)stream_->send(formatter_->format_string(e, p));
}

void NetSink::on_attach(const Channel& chan)
{
    on_attach_(*stream_, chan);
}

bool NetSink::connect(const std::string& server, uint16_t port)
{
    port_ = port;
    server_ = server;
    auto result = net::TCPConnector::connect(server_, port_);
    if (result)
    {
        stream_ = std::move(*result);
        return true;
    }

    return false;
}

} // namespace kb::log

#endif