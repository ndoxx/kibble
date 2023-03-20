#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"
#include "net/tcp_acceptor.h"
#include "net/tcp_connector.h"
#include "net/tcp_stream.h"

#include <thread>

using namespace kb;
using namespace kb::log;

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan_client(Severity::Verbose, "client", "cli", kb::col::darkblue);
    chan_client.attach_sink(console_sink);
    Channel chan_server(Severity::Verbose, "server", "srv", kb::col::darkred);
    chan_server.attach_sink(console_sink);

    // Start a TCP server on a new thread. This could as well be in another application on the same machine, or in a
    // remote machine in Zimbabwe, it does not matter.
    std::thread server([&chan_server]() {
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
            klog(chan_server).error("Cannot start TCPAcceptor");
            return;
        }
        klog(chan_server).info("Starting server");

        // Accept the first connection
        // This is a blocking call
        auto *a_stream = acceptor.accept();
        klog(chan_server).info("Connection accepted");

        // The stream can work with generic char* buffers, but for the sake of the example, we are going to use the
        // function that is specialized for strings.
        // This is a blocking call
        a_stream->receive(buf);

        klog(chan_server).verbose("Received: \"{}\"", buf);

        // Cleanup
        delete a_stream;
    });

    // Now we connect to the server using the same port number
    // There is no need to instantiate TCPConnector, this is a stateless static class
    auto *c_stream = net::TCPConnector::connect("localhost", 9876);

    // If the connection is not successful, no stream is returned
    if (c_stream == nullptr)
    {
        klog(chan_client).error("Cannot connect to server");
        server.join();
        return 0;
    }

    // We made it here, so everything went fine
    klog(chan_client).info("Successfully connected to server, sending message");
    // Let's send some data to the server
    auto sz = c_stream->send("hello there!");

    if (sz == -1)
    {
        klog(chan_client).error("write error");
    }

    // At this point, we're past the blocking call to receive() server-side, so the thread is joinable
    server.join();

    // Cleanup
    delete c_stream;
    return 0;
}
