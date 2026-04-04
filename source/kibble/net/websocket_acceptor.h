#pragma once

#include "kibble/net/error.h"
#include "kibble/net/tcp_acceptor.h"

#include <cstdint>
#include <expected>
#include <memory>

namespace kb::net
{

class WebSocketStream;

/**
 * @brief Accepts incoming WebSocket connections from web clients.
 *
 * Wraps a TCPAcceptor and performs the RFC 6455 HTTP upgrade handshake before
 * handing back a ready-to-use WebSocketStream.
 *
 * Failures are reported as `std::expected<…, WSError>` so the caller decides
 * how to handle them. Non-fatal handshake diagnostics (e.g. a missing or
 * non-localhost Origin header) are optionally surfaced via a `WSWarning*`
 * out-parameter, following the `std::error_code&` convention used by
 * `<filesystem>`.
 *
 * Typical usage:
 * @code
 *   kb::net::WebSocketAcceptor acceptor(9001);
 *   if (auto r = acceptor.start(); !r)
 *   {
 *       fmt::println(stderr, "start failed: {}", r.error().message());
 *       return 1;
 *   }
 *
 *   kb::net::WSWarning warning;
 *   auto ws = acceptor.accept(&warning);
 *   if (!ws)
 *   {
 *       fmt::println(stderr, "accept failed: {}", ws.error().message());
 *       return 1;
 *   }
 *   if (!warning.empty())
 *       fmt::println(stderr, "{}", warning.message());
 *
 *   (*ws)->send_text("Hello!");
 * @endcode
 *
 * @note Linux and Windows implementations are available.
 */
class WebSocketAcceptor
{
public:
    /**
     * @brief Construct a WebSocketAcceptor.
     *
     * @param port    TCP port to listen on (e.g. 9001)
     * @param address Address to bind to. Empty string -> INADDR_ANY.
     */
    explicit WebSocketAcceptor(uint16_t port, const char* address = "");

    // Non-copyable (TCPAcceptor is non-copyable); movable.
    WebSocketAcceptor(const WebSocketAcceptor&) = delete;
    WebSocketAcceptor& operator=(const WebSocketAcceptor&) = delete;
    WebSocketAcceptor(WebSocketAcceptor&&) = default;
    WebSocketAcceptor& operator=(WebSocketAcceptor&&) = default;

    /**
     * @brief Start listening on the configured port.
     *
     * @return void on success, or a WSError wrapping the NetError from the TCP layer.
     */
    [[nodiscard]] std::expected<void, WSError> start();

    /**
     * @brief Accept one incoming TCP connection and perform the WebSocket handshake.
     *
     * Blocks until a client connects. Returns a WSError if the TCP accept fails
     * or if the HTTP upgrade handshake cannot be completed.
     *
     * Non-fatal conditions detected during the handshake (e.g. a missing Origin
     * header, or an Origin that is not localhost) are written to `*warning` when
     * `warning` is non-null. The out-parameter is cleared on entry so callers do
     * not need to initialise it beforehand. When `warning` is null, diagnostics
     * are silently discarded.
     *
     * @param warning optional out-parameter for non-fatal handshake diagnostics;
     *                pass nullptr to ignore warnings entirely.
     * @return unique_ptr<WebSocketStream> on success, or a WSError on failure
     */
    [[nodiscard]] std::expected<std::unique_ptr<WebSocketStream>, WSError> accept(WSWarning* warning = nullptr);

private:
    TCPAcceptor acceptor_;
};

} // namespace kb::net