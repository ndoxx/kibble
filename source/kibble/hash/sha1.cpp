/*
    sha1.hpp - source code of

    ============
    SHA-1 in C++
    ============

    100% Public Domain.

    Original C Code
        -- Steve Reid <steve@edmweb.com>
    Small changes to fit into bglibs
        -- Bruce Guenter <bruce@untroubled.org>
    Translation to simpler C++ Code
        -- Volker Diels-Grabsch <v@njh.eu>
    Safety fixes
        -- Eugene Hopkinson <slowriot at voxelstorm dot com>
    Header-only library
        -- Zlatko Michailov <zlatko@michailov.org>
    Moved functions back to a .cpp file, adapted to Kibble's conventions, added function to get digest as bytes
        -- ndx <notgivingyoumy@mail.com>
*/

#include "kibble/hash/sha1.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace kb::hash
{

namespace
{

constexpr size_t k_block_ints = 16; /* number of 32bit integers per SHA1 block */
constexpr size_t k_block_bytes = k_block_ints * 4;

inline static void reset(uint32_t digest_[], std::string& buffer_, uint64_t& transforms_)
{
    /* SHA1 initialization constants */
    digest_[0] = 0x67452301;
    digest_[1] = 0xefcdab89;
    digest_[2] = 0x98badcfe;
    digest_[3] = 0x10325476;
    digest_[4] = 0xc3d2e1f0;

    /* Reset counters */
    buffer_ = "";
    transforms_ = 0;
}

inline uint32_t rol(const uint32_t value, const size_t bits)
{
    return (value << bits) | (value >> (32 - bits));
}

inline uint32_t blk(const uint32_t block[k_block_ints], const size_t i)
{
    return rol(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ block[(i + 2) & 15] ^ block[i], 1);
}

/*
 * (R0+R1), R2, R3, R4 are the different operations used in SHA1
 */

inline void R0(const uint32_t block[k_block_ints], const uint32_t v, uint32_t& w, const uint32_t x, const uint32_t y,
               uint32_t& z, const size_t i)
{
    z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5);
    w = rol(w, 30);
}

inline void R1(uint32_t block[k_block_ints], const uint32_t v, uint32_t& w, const uint32_t x, const uint32_t y,
               uint32_t& z, const size_t i)
{
    block[i] = blk(block, i);
    z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5);
    w = rol(w, 30);
}

inline void R2(uint32_t block[k_block_ints], const uint32_t v, uint32_t& w, const uint32_t x, const uint32_t y,
               uint32_t& z, const size_t i)
{
    block[i] = blk(block, i);
    z += (w ^ x ^ y) + block[i] + 0x6ed9eba1 + rol(v, 5);
    w = rol(w, 30);
}

inline void R3(uint32_t block[k_block_ints], const uint32_t v, uint32_t& w, const uint32_t x, const uint32_t y,
               uint32_t& z, const size_t i)
{
    block[i] = blk(block, i);
    z += (((w | x) & y) | (w & x)) + block[i] + 0x8f1bbcdc + rol(v, 5);
    w = rol(w, 30);
}

inline void R4(uint32_t block[k_block_ints], const uint32_t v, uint32_t& w, const uint32_t x, const uint32_t y,
               uint32_t& z, const size_t i)
{
    block[i] = blk(block, i);
    z += (w ^ x ^ y) + block[i] + 0xca62c1d6 + rol(v, 5);
    w = rol(w, 30);
}

/*
 * Hash a single 512-bit block. This is the core of the algorithm.
 */

void transform(uint32_t digest_[], uint32_t block[k_block_ints], uint64_t& transforms_)
{
    /* Copy digest_[] to working vars */
    uint32_t a = digest_[0];
    uint32_t b = digest_[1];
    uint32_t c = digest_[2];
    uint32_t d = digest_[3];
    uint32_t e = digest_[4];

    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(block, a, b, c, d, e, 0);
    R0(block, e, a, b, c, d, 1);
    R0(block, d, e, a, b, c, 2);
    R0(block, c, d, e, a, b, 3);
    R0(block, b, c, d, e, a, 4);
    R0(block, a, b, c, d, e, 5);
    R0(block, e, a, b, c, d, 6);
    R0(block, d, e, a, b, c, 7);
    R0(block, c, d, e, a, b, 8);
    R0(block, b, c, d, e, a, 9);
    R0(block, a, b, c, d, e, 10);
    R0(block, e, a, b, c, d, 11);
    R0(block, d, e, a, b, c, 12);
    R0(block, c, d, e, a, b, 13);
    R0(block, b, c, d, e, a, 14);
    R0(block, a, b, c, d, e, 15);
    R1(block, e, a, b, c, d, 0);
    R1(block, d, e, a, b, c, 1);
    R1(block, c, d, e, a, b, 2);
    R1(block, b, c, d, e, a, 3);
    R2(block, a, b, c, d, e, 4);
    R2(block, e, a, b, c, d, 5);
    R2(block, d, e, a, b, c, 6);
    R2(block, c, d, e, a, b, 7);
    R2(block, b, c, d, e, a, 8);
    R2(block, a, b, c, d, e, 9);
    R2(block, e, a, b, c, d, 10);
    R2(block, d, e, a, b, c, 11);
    R2(block, c, d, e, a, b, 12);
    R2(block, b, c, d, e, a, 13);
    R2(block, a, b, c, d, e, 14);
    R2(block, e, a, b, c, d, 15);
    R2(block, d, e, a, b, c, 0);
    R2(block, c, d, e, a, b, 1);
    R2(block, b, c, d, e, a, 2);
    R2(block, a, b, c, d, e, 3);
    R2(block, e, a, b, c, d, 4);
    R2(block, d, e, a, b, c, 5);
    R2(block, c, d, e, a, b, 6);
    R2(block, b, c, d, e, a, 7);
    R3(block, a, b, c, d, e, 8);
    R3(block, e, a, b, c, d, 9);
    R3(block, d, e, a, b, c, 10);
    R3(block, c, d, e, a, b, 11);
    R3(block, b, c, d, e, a, 12);
    R3(block, a, b, c, d, e, 13);
    R3(block, e, a, b, c, d, 14);
    R3(block, d, e, a, b, c, 15);
    R3(block, c, d, e, a, b, 0);
    R3(block, b, c, d, e, a, 1);
    R3(block, a, b, c, d, e, 2);
    R3(block, e, a, b, c, d, 3);
    R3(block, d, e, a, b, c, 4);
    R3(block, c, d, e, a, b, 5);
    R3(block, b, c, d, e, a, 6);
    R3(block, a, b, c, d, e, 7);
    R3(block, e, a, b, c, d, 8);
    R3(block, d, e, a, b, c, 9);
    R3(block, c, d, e, a, b, 10);
    R3(block, b, c, d, e, a, 11);
    R4(block, a, b, c, d, e, 12);
    R4(block, e, a, b, c, d, 13);
    R4(block, d, e, a, b, c, 14);
    R4(block, c, d, e, a, b, 15);
    R4(block, b, c, d, e, a, 0);
    R4(block, a, b, c, d, e, 1);
    R4(block, e, a, b, c, d, 2);
    R4(block, d, e, a, b, c, 3);
    R4(block, c, d, e, a, b, 4);
    R4(block, b, c, d, e, a, 5);
    R4(block, a, b, c, d, e, 6);
    R4(block, e, a, b, c, d, 7);
    R4(block, d, e, a, b, c, 8);
    R4(block, c, d, e, a, b, 9);
    R4(block, b, c, d, e, a, 10);
    R4(block, a, b, c, d, e, 11);
    R4(block, e, a, b, c, d, 12);
    R4(block, d, e, a, b, c, 13);
    R4(block, c, d, e, a, b, 14);
    R4(block, b, c, d, e, a, 15);

    /* Add the working vars back into digest_[] */
    digest_[0] += a;
    digest_[1] += b;
    digest_[2] += c;
    digest_[3] += d;
    digest_[4] += e;

    /* Count the number of transformations */
    transforms_++;
}

void buffer_to_block(const std::string& buffer, uint32_t block[k_block_ints])
{
    /* Convert the std::string (byte buffer) to a uint32_t array (MSB) */
    for (size_t ii = 0; ii < k_block_ints; ii++)
    {
        // clang-format off
        block[ii] =  (uint32_t(buffer[4*ii+3]) & 0xff)
                   | (uint32_t(buffer[4*ii+2]) & 0xff)<<8
                   | (uint32_t(buffer[4*ii+1]) & 0xff)<<16
                   | (uint32_t(buffer[4*ii+0]) & 0xff)<<24;
        // clang-format on
    }
}

} // namespace

SHA1::SHA1()
{
    reset(digest_, buffer_, transforms_);
}

void SHA1::update(const std::string& s)
{
    std::istringstream is(s);
    update(is);
}

void SHA1::update(std::istream& is)
{
    while (true)
    {
        char sbuf[k_block_bytes];
        is.read(sbuf, std::streamsize(k_block_bytes - buffer_.size()));
        buffer_.append(sbuf, std::size_t(is.gcount()));
        if (buffer_.size() != k_block_bytes)
        {
            return;
        }
        uint32_t block[k_block_ints];
        buffer_to_block(buffer_, block);
        transform(digest_, block, transforms_);
        buffer_.clear();
    }
}

/*
 * Apply padding and the length block, then run the final transform(s).
 * After this call digest_[] contains the result and the object is reset.
 */
void SHA1::finalize()
{
    /* Total number of hashed bits */
    uint64_t total_bits = (transforms_ * k_block_bytes + buffer_.size()) * 8;

    /* Padding */
    buffer_ += char(0x80);
    size_t orig_size = buffer_.size();
    while (buffer_.size() < k_block_bytes)
    {
        buffer_ += char(0x00);
    }

    uint32_t block[k_block_ints];
    buffer_to_block(buffer_, block);

    if (orig_size > k_block_bytes - 8)
    {
        transform(digest_, block, transforms_);
        for (size_t i = 0; i < k_block_ints - 2; i++)
        {
            block[i] = 0;
        }
    }

    /* Append total_bits, split this uint64_t into two uint32_t */
    block[k_block_ints - 1] = uint32_t(total_bits);
    block[k_block_ints - 2] = uint32_t(total_bits >> 32u);
    transform(digest_, block, transforms_);

    /* Reset for next run */
    // Note: we reset buffer_ and transforms_ but intentionally leave digest_[]
    // intact so the callers (final() / final_bytes()) can read it immediately after.
    buffer_ = "";
    transforms_ = 0;
}

std::string SHA1::final()
{
    finalize();

    std::ostringstream result;
    for (size_t ii = 0; ii < sizeof(digest_) / sizeof(digest_[0]); ii++)
    {
        result << std::hex << std::setfill('0') << std::setw(8);
        result << digest_[ii];
    }

    reset(digest_, buffer_, transforms_);
    return result.str();
}

std::array<uint8_t, 20> SHA1::final_bytes()
{
    finalize();

    // Unpack the five 32-bit words into 20 raw bytes (big-endian, per SHA-1 spec).
    std::array<uint8_t, 20> result{};
    for (size_t ii = 0; ii < 5; ++ii)
    {
        result[ii * 4 + 0] = static_cast<uint8_t>((digest_[ii] >> 24u) & 0xFFu);
        result[ii * 4 + 1] = static_cast<uint8_t>((digest_[ii] >> 16u) & 0xFFu);
        result[ii * 4 + 2] = static_cast<uint8_t>((digest_[ii] >> 8u) & 0xFFu);
        result[ii * 4 + 3] = static_cast<uint8_t>(digest_[ii] & 0xFFu);
    }

    reset(digest_, buffer_, transforms_);
    return result;
}

std::string SHA1::from_file(const std::string& filename)
{
    std::ifstream stream(filename.c_str(), std::ios::binary);
    SHA1 checksum;
    checksum.update(stream);
    return checksum.final();
}

} // namespace kb::hash