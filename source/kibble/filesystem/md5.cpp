#include "filesystem/md5.h"
#include "algorithm/endian.h"
#include "assert/assert.h"

#include "fmt/core.h"
#include <cstring>

namespace kb
{

// T[i] = int(4294967296 * abs(sin(i))), with i in radians
constexpr uint32_t k_T[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

// Basic MD5 step functions for each round
constexpr inline uint32_t F(uint32_t round, uint32_t x, uint32_t y, uint32_t z)
{
    switch (round)
    {
    case 0:
        return (x & y) | (~x & z);
    case 1:
        return (x & z) | (y & ~z);
    case 2:
        return x ^ y ^ z;
    case 3:
        return y ^ (x | ~z);
    default:
        return 0;
    }
}

// clang-format off
// Constants for the MD5 Transform routine as defined in RFC 1321
constexpr std::array<uint32_t, 64> k_S = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

constexpr std::array<uint32_t, 64> k_K = {
    0, 1, 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 
    1, 6, 11, 0,  5,  10, 15, 4,  9,  14, 3,  8,  13, 2,  7,  12,
    5, 8, 11, 14, 1,  4,  7,  10, 13, 0,  3,  6,  9,  12, 15, 2,  
    0, 7, 14, 5,  12, 3,  10, 1,  8,  15, 6, 13,  4,  11, 2,  9
};
// clang-format on

// Function to perform the cyclic left rotation of blocks of data
inline constexpr uint32_t ROT32L(uint32_t data, uint32_t shift_bits)
{
    return (data << shift_bits) | (data >> (32u - shift_bits));
}

// The MD5 transformation for all four rounds.
inline constexpr void transform(uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t round, uint32_t x, uint32_t t,
                                uint32_t s)
{
    // fmt::print("A: {:08x} B: {:08x} C: {:08x} D: {:08x} Xk: {:08x} s: {:02d} T: {:08x} ", A, B, C, D, x, s, t);
    A = B + ROT32L(A + F(round, B, C, D) + x + t, s);
    // fmt::print("A: {:08x}\n", A);
}

// Calculate the correct state index permutation for each operation
inline constexpr uint32_t perm(uint32_t idx, uint32_t ii)
{
    return (idx + ii * 3) % 4;
}

md5::md5(const void* input, size_t length)
{
    process(input, length);
    finish();
}

void md5::process(const void* input, size_t length)
{
    K_ASSERT(!finished_, "MD5 already finished.");

    // If not enough data to complete a block, stash and return
    if (head_ + length < k_block_size)
    {
        std::memcpy(buffer_.data() + head_, input, length);
        head_ += length;
        return;
    }

    // Because we did not branch on the above, we know that length >= k_block_size - head_
    uint32_t processed = k_block_size - head_;
    std::memcpy(buffer_.data() + head_, input, processed);
    // head_ will be reset anyway, so we don't need to update it
    process_block(0);

    // Remaining size is length - processed
    // Process data block by block while there is enough data
    while (length - processed >= k_block_size)
    {
        std::memcpy(buffer_.data(), reinterpret_cast<const uint8_t*>(input) + processed, k_block_size);
        processed += k_block_size;
        process_block(0);
    }

    // Store remaining bytes for next time
    if (processed != length)
    {
        std::memcpy(buffer_.data(), reinterpret_cast<const uint8_t*>(input) + processed, length - processed);
        head_ = uint32_t(length) - processed;
    }
}

void md5::finish()
{
    K_ASSERT(!finished_, "MD5 already finished.");

    length_ += head_;

    /*
        Merkle–Damgård length padding / strengthening

        The total length to process can exceed k_block_size when head is initially
        between 56 and 64 bytes. This is why buffer_ has a size of two times k_block_size,
        so we can write padding bytes to it linearly without worrying about a stack overflow.
        If the frame_length does exceed k_block_size, we'll have an additional block to
        process later on.
    */

    // Pad with a single 1 followed by as many 0 bits as needed to make the
    // length 8 bytes shy of a multiple of k_block_size
    int32_t pad = int32_t(k_block_size - 2 * sizeof(uint32_t) - head_);
    if (pad <= 0)
    {
        pad += k_block_size;
    }

    buffer_[head_] = 0x80;
    if (pad > 1)
    {
        std::memset(buffer_.data() + head_ + 1, 0, size_t(pad - 1));
    }
    head_ += uint32_t(pad);

    // Write original message length (in bits), now the total length is exactly congruent
    // to 0 mod k_block_size bytes.
    uint64_t len_bits = length_ * 8;
    std::memcpy(buffer_.data() + head_, &len_bits, sizeof(uint64_t));
    head_ += sizeof(uint64_t);

    // We have either one or two blocks to process.
    uint32_t num_blocks = head_ / k_block_size;

    for (uint32_t ii = 0; ii < num_blocks; ++ii)
    {
        process_block(ii);
    }

    finished_ = true;
}

/* 4 rounds of 16 operations on input data.
 * See RFC 1321, 3.4 Step 4.
 * Let [abcd k s i] denote the operation a = b + ((a + F(0,b,c,d) + X[k] + T[i]) <<< s), do:
 * [ABCD  0  7  1]  [DABC  1 12  2]  [CDAB  2 17  3]  [BCDA  3 22  4]
 * [ABCD  4  7  5]  [DABC  5 12  6]  [CDAB  6 17  7]  [BCDA  7 22  8]
 * [ABCD  8  7  9]  [DABC  9 12 10]  [CDAB 10 17 11]  [BCDA 11 22 12]
 * [ABCD 12  7 13]  [DABC 13 12 14]  [CDAB 14 17 15]  [BCDA 15 22 16]
 *
 * Let [abcd k s i] denote the operation a = b + ((a + F(1,b,c,d) + X[k] + T[i]) <<< s), do:
 * [ABCD  1  5 17]  [DABC  6  9 18]  [CDAB 11 14 19]  [BCDA  0 20 20]
 * [ABCD  5  5 21]  [DABC 10  9 22]  [CDAB 15 14 23]  [BCDA  4 20 24]
 * [ABCD  9  5 25]  [DABC 14  9 26]  [CDAB  3 14 27]  [BCDA  8 20 28]
 * [ABCD 13  5 29]  [DABC  2  9 30]  [CDAB  7 14 31]  [BCDA 12 20 32]
 *
 * Let [abcd k s i] denote the operation a = b + ((a + F(2,b,c,d) + X[k] + T[i]) <<< s), do:
 * [ABCD  5  4 33]  [DABC  8 11 34]  [CDAB 11 16 35]  [BCDA 14 23 36]
 * [ABCD  1  4 37]  [DABC  4 11 38]  [CDAB  7 16 39]  [BCDA 10 23 40]
 * [ABCD 13  4 41]  [DABC  0 11 42]  [CDAB  3 16 43]  [BCDA  6 23 44]
 * [ABCD  9  4 45]  [DABC 12 11 46]  [CDAB 15 16 47]  [BCDA  2 23 48]
 *
 * Let [abcd k s i] denote the operation a = b + ((a + F(3,b,c,d) + X[k] + T[i]) <<< s), do:
 * [ABCD  0  6 49]  [DABC  7 10 50]  [CDAB 14 15 51]  [BCDA  5 21 52]
 * [ABCD 12  6 53]  [DABC  3 10 54]  [CDAB 10 15 55]  [BCDA  1 21 56]
 * [ABCD  8  6 57]  [DABC 15 10 58]  [CDAB  6 15 59]  [BCDA 13 21 60]
 * [ABCD  4  6 61]  [DABC 11 10 62]  [CDAB  2 15 63]  [BCDA  9 21 64]
 */
void md5::process_block(uint32_t block_offset)
{
    length_ += k_block_size;

    auto state = state_;
    const uint32_t* block32 = reinterpret_cast<const uint32_t*>(buffer_.data() + block_offset * k_block_size);

    for (uint32_t kk = 0; kk < 64; ++kk)
    {
        transform(state_[perm(0, kk)], // A, B, C or D (circular permutation)
                  state_[perm(1, kk)], // A, B, C or D (circular permutation)
                  state_[perm(2, kk)], // A, B, C or D (circular permutation)
                  state_[perm(3, kk)], // A, B, C or D (circular permutation)
                  kk / 16,             // round (0 to 3)
                  block32[k_K[kk]],    // Xk (data at appropriate offset)
                  k_T[kk],             // T (constant)
                  k_S[kk]              // s (shift)
        );
    }

    for (uint32_t ii = 0; ii < 4; ++ii)
    {
        state_[ii] += state[ii];
    }

    head_ = 0;
}

std::string md5::to_string() const
{
    return fmt::format("{:08x}{:08x}{:08x}{:08x}", bswap(state_[0]), bswap(state_[1]), bswap(state_[2]),
                       bswap(state_[3]));
}

} // namespace kb