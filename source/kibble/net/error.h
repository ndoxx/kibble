#pragma once

#include <string>
#include <variant>

namespace kb::net
{

/**
* @brief Carries the OS error code and a human-readable description.
* Construct it right after a failing syscall so the code is still fresh.
* 
*/
struct NetError
{
    int code;            ///< errno (Linux) or WSAGetLastError() (Windows)
    std::string context; ///< what operation failed, e.g. "socket()"

    /// @brief Build a NetError from the *current* OS error (errno / WSAGetLastError).
    static NetError current(std::string context);

    /// @brief Human-readable string, e.g. "bind() failed [98]: Address already in use"
    [[nodiscard]] std::string message() const;
};

/**
 * @brief Error type for the WebSocket layer.
 *
 * Distinguishes two failure sources:
 *  - `NetError`    - an OS-level TCP I/O failure (errno / WSAGetLastError).
 *  - `std::string` - a WebSocket protocol violation (pure C++, no OS code).
 *
 * Use WSError::tcp() and WSError::protocol() to construct, and message() or
 * the fmtlib formatter to format.
 */
struct WSError
{
    std::variant<NetError, std::string> cause;

    /// @brief Wrap a TCP-layer error.
    static WSError tcp(NetError e);

    /// @brief Describe a protocol-level violation.
    static WSError protocol(std::string msg);

    /// @brief Human-readable string
    [[nodiscard]] std::string message() const;
};

/**
 * @brief Non-fatal diagnostic from the WebSocket handshake.
 *
 * Carries conditions that are policy violations worth logging but that the
 * server has chosen not to treat as hard errors (e.g. a non-localhost Origin
 * header in a dev-server scenario, or a missing Origin header entirely).
 *
 * Passed as an optional out-parameter to WebSocketAcceptor::accept():
 *
 * @code
 *   kb::net::WSWarning warning;
 *   auto ws = acceptor.accept(&warning);
 *   if (ws && !warning.empty())
 *       fmt::println(stderr, "handshake warning: {}", warning.message());
 * @endcode
 *
 * If the pointer is null, warnings are silently discarded.
 */
struct WSWarning
{
    std::string text; ///< empty when no warning was recorded

    /// @brief True when no warning has been set.
    [[nodiscard]] bool empty() const { return text.empty(); }

    /// @brief Human-readable string, e.g. "WS handshake warning: unexpected Origin: http://example.com"
    [[nodiscard]] std::string message() const;
};

} // namespace kb::net