#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "net/tcp_acceptor.h"
#include "net/tcp_connector.h"
#include "net/tcp_stream.h"

#include <thread>

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("client", 3));
    KLOGGER(create_channel("server", 3));

    KLOGGER(set_channel_tag("client", "cli", kb::col::darkblue));
    KLOGGER(set_channel_tag("server", "srv", kb::col::darkred));

    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    // Start a TCP server on a new thread. This could as well be in another application on the same machine, or in a
    // remote machine in Zimbabwe, it does not matter.
    std::thread server([]() {
        // Configure this socket to listen on port 9876
        // The address is left empty (by default), so the socket will be bound to all available interfaces, and
        // connections from any address will be accepted. We could as well use the loopback address or "localhost" to
        // accept only local connections.
        auto acceptor = net::TCPAcceptor(9876);
        // The TCP stream will write to this buffer
        std::string buf;

        // Start listening
        if (!acceptor.start())
        {
            KLOGE("server") << "Cannot start TCPAcceptor" << std::endl;
            return;
        }
        KLOG("server", 1) << "Starting server" << std::endl;

        // Accept the first connection
        // This is a blocking call
        auto *a_stream = acceptor.accept();
        KLOG("server", 1) << "Connection accepted" << std::endl;

        // The stream can work with generic char* buffers, but for the sake of the example, we are going to use the
        // function that is specialized for strings.
        // This is a blocking call
        a_stream->receive(buf);

        KLOG("server", 1) << "Received: \"" << buf << "\"" << std::endl;

        // Cleanup
        delete a_stream;
    });

    // Now we connect to the server using the same port number
    // There is no need to instantiate TCPConnector, this is a stateless static class
    auto *c_stream = net::TCPConnector::connect("localhost", 9876);

    // If the connection is not successful, no stream is returned
    if (c_stream == nullptr)
    {
        KLOGE("client") << "Cannot connect to server" << std::endl;
        server.join();
        return 0;
    }

    // We made it here, so everything went fine
    KLOG("client", 1) << "Successfully connected to server, sending message" << std::endl;
    // Let's send some data to the server
    auto sz = c_stream->send("hello there!");

    if (sz == -1)
    {
        KLOGE("client") << "write error" << std::endl;
    }

    // At this point, we're past the blocking call to receive() server-side, so the thread is joinable
    server.join();

    // Cleanup
    delete c_stream;
    return 0;
}
