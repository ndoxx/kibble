#include "kibble/string/base64.h"
#include <array>
#include <cstdint>

namespace kb::su
{

namespace
{

// Lookup tables as constexpr arrays for better type safety
static constexpr std::array<char, 64> k_base64_chars = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
    's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

static constexpr std::array<int8_t, 256> k_base64_decode_vals = []() constexpr {
    std::array<int8_t, 256> vals{};
    for (auto& v : vals)
    {
        v = -1;
    }

    vals['+'] = 62;
    vals['/'] = 63;
    for (int8_t ii = 0; ii < 26; ++ii)
    {
        vals[size_t('A' + ii)] = ii;
        vals[size_t('a' + ii)] = 26 + ii;
    }
    for (int8_t ii = 0; ii < 10; ++ii)
    {
        vals[size_t('0' + ii)] = 52 + ii;
    }
    return vals;
}();

} // namespace

std::string base64_encode(std::span<const std::byte> data)
{
    if (data.empty())
    {
        return {};
    }

    const size_t output_size = ((data.size() + 2) / 3) * 4;
    std::string out;
    out.reserve(output_size);

    size_t ii = 0;
    const size_t full_triplets = data.size() / 3;

    // Process full triplets (3 bytes -> 4 chars)
    for (size_t triplet = 0; triplet < full_triplets; ++triplet, ii += 3)
    {
        const uint32_t val = (static_cast<uint32_t>(data[ii]) << 16) | (static_cast<uint32_t>(data[ii + 1]) << 8) |
                             static_cast<uint32_t>(data[ii + 2]);

        out.push_back(k_base64_chars[(val >> 18) & 0x3F]);
        out.push_back(k_base64_chars[(val >> 12) & 0x3F]);
        out.push_back(k_base64_chars[(val >> 6) & 0x3F]);
        out.push_back(k_base64_chars[val & 0x3F]);
    }

    // Handle remaining bytes
    const size_t remaining = data.size() - ii;
    if (remaining > 0)
    {
        const uint32_t val =
            (static_cast<uint32_t>(data[ii]) << 16) | (remaining > 1 ? (static_cast<uint32_t>(data[ii + 1]) << 8) : 0);

        out.push_back(k_base64_chars[(val >> 18) & 0x3F]);
        out.push_back(k_base64_chars[(val >> 12) & 0x3F]);
        out.push_back(remaining > 1 ? k_base64_chars[(val >> 6) & 0x3F] : '=');
        out.push_back('=');
    }

    return out;
}

std::string base64_decode(std::string_view data)
{
    if (data.empty())
    {
        return {};
    }

    // Remove padding for size calculation
    size_t input_len = data.size();
    while (input_len > 0 && data[input_len - 1] == '=')
    {
        --input_len;
    }

    const size_t output_size = (input_len * 3) / 4;
    std::string out;
    out.reserve(output_size);

    uint32_t val = 0;
    int bits = 0;

    for (size_t ii = 0; ii < input_len; ++ii)
    {
        const int8_t decode_val = k_base64_decode_vals[static_cast<uint8_t>(data[ii])];
        if (decode_val == -1)
        {
            break; // Invalid character
        }

        val = (val << 6) | static_cast<uint32_t>(decode_val);
        bits += 6;

        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
        }
    }

    return out;
}

} // namespace kb::su