#include "kibble/net/websocket_stream.h"
#include "kibble/algorithm/endian.h" // kb::bswap
#include "kibble/net/tcp_stream.h"
#include "kibble/platform/endian.h" // LITTLEENDIAN / BIGENDIAN macros

#include <array>
#include <cstring>

// SIMD unmask: use SSE2 XOR instructions when the compiler target includes
// SSE4.2 (our stated baseline). The _mm_xor_si128 intrinsic itself only
// requires SSE2, but we gate on SSE4.2 so that the same preprocessor symbol
// used elsewhere in the project acts as the single capability knob.
//
// MSVC: SSE4.2 is implied by /arch:AVX or higher; __SSE4_2__ is defined by
//       clang-cl and by MSVC >= 19.28 with the appropriate /arch flag.
//       The _MSC_VER + __AVX__ guard below covers the common MSVC/AVX case
//       where __SSE4_2__ may not be set explicitly.
#if defined(__SSE4_2__) || (defined(_MSC_VER) && defined(__AVX__))
#define KB_HAS_SSE4_2 1
#include <immintrin.h>
#endif

namespace kb::net
{

namespace
{

// ---- Frame encoding constants -------------------------------------------

constexpr uint8_t k_fin_bit = 0x80u;
constexpr uint8_t k_opcode_mask = 0x0Fu;
constexpr uint8_t k_mask_bit = 0x80u;
constexpr uint8_t k_paylen_mask = 0x7Fu;
constexpr uint8_t k_rsv_mask = 0x70u;

constexpr uint8_t k_paylen_16 = 126u;
constexpr uint8_t k_paylen_64 = 127u;

constexpr size_t k_mask_key_len = 4;

// ---- Helpers --------------------------------------------------

// WebSocket length fields are big-endian (network byte order, RFC 6455 §5.2).
// On a big-endian host the in-memory representation already matches the wire
// format, so a plain memcpy suffices. On a little-endian host the bytes must
// be reversed before writing and after reading. kb::bswap<T> always reverses
// unconditionally and is constexpr, so the compiler folds the swap away
// entirely on big-endian builds where the LITTLEENDIAN macro is absent.

/**
 * @internal
 * @brief Serialise a 16-bit unsigned integer as big-endian into a 2-byte buffer.
 *
 * On little-endian hosts, `kb::bswap<uint16_t>` emits a single `bswap`
 * instruction; the result is written via `memcpy` to avoid strict-aliasing
 * violations. On big-endian hosts the swap is compiled away and only the
 * store remains.
 */
inline void write_be16(char* buf, uint16_t value)
{
#if defined(LITTLEENDIAN)
    value = kb::bswap<uint16_t>(value);
#endif
    std::memcpy(buf, &value, sizeof(value));
}

/**
 * @internal
 * @brief Serialise a 64-bit unsigned integer as big-endian into an 8-byte buffer.
 *
 * Same approach as `write_be16`.
 */
inline void write_be64(char* buf, uint64_t value)
{
#if defined(LITTLEENDIAN)
    value = kb::bswap<uint64_t>(value);
#endif
    std::memcpy(buf, &value, sizeof(value));
}

/**
 * @internal
 * @brief Deserialise a big-endian 2-byte buffer into a 16-bit unsigned integer.
 *
 * `memcpy` into a temporary satisfies strict-aliasing rules; the compiler
 * eliminates it and emits a direct load. On little-endian hosts `kb::bswap`
 * then reverses the bytes to host order; on big-endian hosts the swap is a
 * no-op and only the load remains.
 */
inline uint16_t read_be16(const char* buf)
{
    uint16_t value;
    std::memcpy(&value, buf, sizeof(value));
#if defined(LITTLEENDIAN)
    value = kb::bswap<uint16_t>(value);
#endif
    return value;
}

/**
 * @internal
 * @brief Deserialise a big-endian 8-byte buffer into a 64-bit unsigned integer.
 *
 * Same approach as `read_be16`.
 */
inline uint64_t read_be64(const char* buf)
{
    uint64_t value;
    std::memcpy(&value, buf, sizeof(value));
#if defined(LITTLEENDIAN)
    value = kb::bswap<uint64_t>(value);
#endif
    return value;
}

/**
 * @internal
 * @brief XOR every byte of `payload` with the repeating 4-byte masking key.
 *
 * Three implementations are selected at compile time:
 *
 * 1. SSE4.2 path (KB_HAS_SSE4_2):
 *    The 4-byte key is broadcast to a 128-bit register with _mm_set1_epi32,
 *    then 16 bytes at a time are XORed with unaligned load/store intrinsics.
 *    Processing 16 bytes per iteration reduces loop overhead by 16× compared
 *    to the byte-at-a-time approach. Remaining bytes (< 16) are handled by
 *    the scalar tail.
 *
 * 2. Scalar 8-byte path (fallback, no SIMD):
 *    The 4-byte key is replicated into a uint64_t (key32 | key32 << 32) and
 *    used to XOR 8 bytes at a time via memcpy/memcpy to avoid strict-aliasing
 *    violations. This halves loop iterations versus a 4-byte approach and is
 *    safe on all platforms without any SIMD support.
 *
 * 3. Byte tail (both paths):
 *    The final 0-15 (SSE) or 0-7 (scalar) bytes are handled one at a time.
 *
 * Both chunk sizes (16 and 8) are exact multiples of the 4-byte key length,
 * so the key phase is always zero at the start of each tail - `key[ii % 4]`
 * in the tail correctly resumes at `key[0]` without any extra bookkeeping.
 */
void unmask_payload(std::string& payload, const std::array<char, k_mask_key_len>& key)
{
    char* data = payload.data();
    const size_t len = payload.size();

    uint32_t key32 = 0;
    std::memcpy(&key32, key.data(), 4);

#if defined(KB_HAS_SSE4_2)
    // --- SSE4.2 path: 16 bytes per iteration ---
    const __m128i key_vec = _mm_set1_epi32(static_cast<int>(key32));

    size_t ii = 0;
    for (; ii + 16 <= len; ii += 16)
    {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + ii));
        chunk = _mm_xor_si128(chunk, key_vec);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + ii), chunk);
    }
    // Scalar tail for the remaining < 16 bytes.
    for (; ii < len; ++ii)
    {
        data[ii] ^= key[ii % k_mask_key_len];
    }

#else
    // --- Scalar 8-byte path: 8 bytes per iteration ---
    // Replicate the 32-bit key into both halves of a 64-bit word.
    const uint64_t key64 = static_cast<uint64_t>(key32) | (static_cast<uint64_t>(key32) << 32u);

    size_t ii = 0;
    for (; ii + 8 <= len; ii += 8)
    {
        uint64_t chunk;
        std::memcpy(&chunk, data + ii, 8);
        chunk ^= key64;
        std::memcpy(data + ii, &chunk, 8);
    }
    // Scalar tail for the remaining < 8 bytes.
    for (; ii < len; ++ii)
    {
        data[ii] ^= key[ii % k_mask_key_len];
    }
#endif
}

} // namespace

// ---- Construction / destruction -----------------------------------------

WebSocketStream::WebSocketStream(std::unique_ptr<TCPStream> stream) : stream_(std::move(stream))
{
}

WebSocketStream::~WebSocketStream()
{
    // Best-effort: ignore the result - destructors must not throw / propagate.
    (void)send_close(); // NOLINT(bugprone-unused-return-value)
}

uint16_t WebSocketStream::get_peer_port() const
{
    return stream_->get_peer_port();
}

const std::string& WebSocketStream::get_peer_ip() const
{
    return stream_->get_peer_ip();
}

// ---- Internal send_frame ------------------------------------------------

std::expected<void, WSError> WebSocketStream::send_frame(WSOpcode opcode, const char* payload, size_t len, bool fin)
{
    // Maximum header: 2 base + 8 extended length = 10 bytes.
    // Server frames are never masked.
    std::array<char, 10> header{};
    size_t header_len = 0;

    header[0] = static_cast<char>((fin ? k_fin_bit : 0x00u) | static_cast<uint8_t>(opcode));
    header_len = 1;

    if (len < k_paylen_16)
    {
        header[1] = static_cast<char>(len);
        header_len = 2;
    }
    else if (len <= 0xFFFFu)
    {
        header[1] = static_cast<char>(k_paylen_16);
        write_be16(&header[2], static_cast<uint16_t>(len));
        header_len = 4;
    }
    else
    {
        header[1] = static_cast<char>(k_paylen_64);
        write_be64(&header[2], static_cast<uint64_t>(len));
        header_len = 10;
    }

    if (auto r = stream_->send(header.data(), header_len); !r)
    {
        return std::unexpected(WSError::tcp(r.error()));
    }
    if (len > 0 && payload != nullptr)
    {
        if (auto r = stream_->send(payload, len); !r)
        {
            return std::unexpected(WSError::tcp(r.error()));
        }
    }
    return {};
}

// ---- Public send API ----------------------------------------------------

std::expected<void, WSError> WebSocketStream::send_text(const std::string& msg)
{
    return send_frame(WSOpcode::text, msg.data(), msg.size());
}

std::expected<void, WSError> WebSocketStream::send_binary(const char* data, size_t len)
{
    return send_frame(WSOpcode::binary, data, len);
}

std::expected<void, WSError> WebSocketStream::send_close(uint16_t code)
{
    if (closed_)
    {
        return {};
    }
    closed_ = true;

    char body[2];
    write_be16(body, code);
    return send_frame(WSOpcode::close, body, sizeof(body));
}

std::expected<void, WSError> WebSocketStream::send_pong(const std::string& payload)
{
    return send_frame(WSOpcode::pong, payload.data(), payload.size());
}

// ---- read_frame ---------------------------------------------------------

std::expected<void, WSError> WebSocketStream::read_frame(WSOpcode& opcode, std::string& payload, bool& fin)
{
    // Mandatory 2-byte header.
    char header[2];
    if (auto r = stream_->receive_exact(header, sizeof(header)); !r)
    {
        return std::unexpected(WSError::tcp(r.error()));
    }

    const uint8_t byte0 = static_cast<uint8_t>(header[0]);
    const uint8_t byte1 = static_cast<uint8_t>(header[1]);

    fin = (byte0 & k_fin_bit) != 0;
    opcode = static_cast<WSOpcode>(byte0 & k_opcode_mask);

    // RSV bits must be zero when no extension is negotiated (RFC 6455 §5.2).
    if ((byte0 & k_rsv_mask) != 0)
    {
        return std::unexpected(WSError::protocol("non-zero RSV bits - no extension negotiated"));
    }

    const bool masked = (byte1 & k_mask_bit) != 0;
    const uint8_t raw_pay_len = byte1 & k_paylen_mask;

    // Determine actual payload length.
    uint64_t pay_len = 0;
    if (raw_pay_len < k_paylen_16)
    {
        pay_len = raw_pay_len;
    }
    else if (raw_pay_len == k_paylen_16)
    {
        char ext[2];
        if (auto r = stream_->receive_exact(ext, sizeof(ext)); !r)
        {
            return std::unexpected(WSError::tcp(r.error()));
        }
        pay_len = read_be16(ext);
        // RFC 6455 §5.2: must use the minimal encoding.
        if (pay_len <= 125u)
        {
            return std::unexpected(WSError::protocol("non-minimal payload length encoding (16-bit used for " +
                                                     std::to_string(pay_len) + ")"));
        }
    }
    else // k_paylen_64
    {
        char ext[8];
        if (auto r = stream_->receive_exact(ext, sizeof(ext)); !r)
        {
            return std::unexpected(WSError::tcp(r.error()));
        }
        pay_len = read_be64(ext);
        if ((pay_len & (uint64_t{1} << 63u)) != 0)
        {
            return std::unexpected(WSError::protocol("MSB set in 64-bit payload length"));
        }
        if (pay_len <= 0xFFFFu)
        {
            return std::unexpected(WSError::protocol("non-minimal payload length encoding (64-bit used for " +
                                                     std::to_string(pay_len) + ")"));
        }
    }

    // Control frames must not be fragmented and must not exceed 125 bytes (RFC 6455 §5.5).
    const bool is_control = static_cast<uint8_t>(opcode) >= static_cast<uint8_t>(WSOpcode::close);
    if (is_control)
    {
        if (!fin)
        {
            return std::unexpected(WSError::protocol("fragmented control frame"));
        }
        if (pay_len > 125u)
        {
            return std::unexpected(
                WSError::protocol("control frame payload exceeds 125 bytes (" + std::to_string(pay_len) + ")"));
        }
    }

    // Read optional masking key (client->server frames are always masked per RFC 6455 §5.3).
    std::array<char, k_mask_key_len> mask_key{};
    if (masked)
    {
        if (auto r = stream_->receive_exact(mask_key.data(), k_mask_key_len); !r)
        {
            return std::unexpected(WSError::tcp(r.error()));
        }
    }

    // Read payload.
    payload.resize(pay_len);
    if (pay_len > 0)
    {
        if (auto r = stream_->receive_exact(payload.data(), pay_len); !r)
        {
            return std::unexpected(WSError::tcp(r.error()));
        }
        if (masked)
        {
            unmask_payload(payload, mask_key);
        }
    }

    return {};
}

// ---- receive_message ----------------------------------------------------

std::expected<WSOpcode, WSError> WebSocketStream::receive_message(std::string& payload)
{
    payload.clear();
    bool first_frame = true;
    WSOpcode message_opcode = WSOpcode::continuation;

    while (true)
    {
        // Reuse the member buffer to avoid per-call heap allocation.
        // read_frame() calls resize() on it, so capacity is retained across frames.
        WSOpcode frame_opcode = WSOpcode::continuation;
        bool fin = false;

        if (auto r = read_frame(frame_opcode, frame_payload_, fin); !r)
        {
            return std::unexpected(r.error());
        }

        // Control frames may interleave data fragments (RFC 6455 §5.5).
        if (frame_opcode == WSOpcode::close)
        {
            // Echo the close and report it as a clean termination (not an error).
            (void)send_close(); // NOLINT(bugprone-unused-return-value)
            return WSOpcode::close;
        }
        if (frame_opcode == WSOpcode::ping)
        {
            if (auto r = send_pong(frame_payload_); !r)
            {
                return std::unexpected(r.error());
            }
            continue;
        }
        if (frame_opcode == WSOpcode::pong)
        {
            // Unsolicited pong - ignore (RFC 6455 §5.5.3).
            continue;
        }

        // Data frames.
        if (first_frame)
        {
            message_opcode = frame_opcode;
            first_frame = false;

            if (fin)
            {
                // Common case: single-frame, unfragmented message.
                // Swap instead of copy - payload takes ownership of the buffer's
                // heap allocation; frame_payload_ gets payload's (empty, cleared)
                // storage and will be refilled by the next receive_message() call.
                std::swap(payload, frame_payload_);
                return message_opcode;
            }

            // First frame of a multi-frame message: move the content into payload
            // so that subsequent continuation frames can append to it directly.
            payload = std::move(frame_payload_);
        }
        else
        {
            if (frame_opcode != WSOpcode::continuation)
            {
                return std::unexpected(WSError::protocol("expected continuation frame, got opcode " +
                                                         std::to_string(static_cast<int>(frame_opcode))));
            }

            payload.append(frame_payload_);

            if (fin)
            {
                return message_opcode;
            }
        }
    }
}

} // namespace kb::net