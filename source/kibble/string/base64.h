#pragma once

#include <span>
#include <string>
#include <string_view>

namespace kb::su
{

/**
 * @brief Base64-encode some data.
 * 
 * @param data 
 * @return std::string 
 */
[[nodiscard]] std::string base64_encode(std::span<const std::byte> data);

/// @brief Convenience overload for char data
[[nodiscard]] inline std::string base64_encode(std::span<const char> data)
{
    return base64_encode(std::as_bytes(data));
}

/// @brief Convenience overload for unsigned char data
[[nodiscard]] inline std::string base64_encode(std::span<const unsigned char> data)
{
    return base64_encode(std::as_bytes(data));
}

/**
 * @brief Decode a Base64-encoded string.
 * 
 * @param data 
 * @return std::string 
 */
[[nodiscard]] std::string base64_decode(std::string_view data);

} // namespace kb::su