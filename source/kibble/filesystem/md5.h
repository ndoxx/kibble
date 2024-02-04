#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace kb::md5
{

struct context_t
{
    std::string to_string() const;

    std::array<uint32_t, 4> state{
        0x67452301,
        0xefcdab89,
        0x98badcfe,
        0x10325476,
    };
    std::array<uint32_t, 2> length{0, 0};
    std::array<uint32_t, 16> block;
    std::array<uint8_t, 128> buffer;
    bool finished{false};
    uint32_t stored_size{0};
};

void process(context_t& ctx, const void* input, size_t length);

void finish(context_t& ctx);

} // namespace kb::md5