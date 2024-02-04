#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace kb
{

/**
 * @brief Compute the MD5 hash of arbitrary data.
 * 
 */
class md5
{
public:
    /**
     * @brief Use this to progressively calculate an MD5 hash.
     * 
     * Call process() as many times as needed to feed data and finish() when
     * you're done. The digest can be obtained as an array of four 32-bit
     * integers, or as a hex string.
     */
    md5() = default;

    /**
     * @brief Use this to compute an MD5 hash in one go.
     * 
     * @param input input buffer
     * @param length size to read from input
     */
    md5(const void* input, size_t length);

    /**
     * @brief Update the hash with more data.
     * 
     * @param input input buffer
     * @param length size to read from input
     */
    void process(const void* input, size_t length);

    /**
     * @brief Finish the hash.
     * 
     * One called, the object cannot be used to process data anymore.
     * The digest can be obtained using the getters.
     * 
     */
    void finish();

    /**
     * @brief Return a 32 char hex-string representation of the digest.
     * 
     * @return std::string 
     */
    std::string to_string() const;

    /**
     * @brief Get the digest as an array of four 32-bit integers.
     * 
     * @return auto 
     */
    inline auto get_signature() const
    {
        return state_;
    }

private:
    /**
     * @internal
     * @brief Process all the data in the block.
     * 
     * Resets head.
     */
    void process_block();

    /**
     * @brief Update message length representation.
     * 
     * @param increment 
     */
    void update_length(uint32_t increment);

private:
    static constexpr uint32_t k_block_size = 64;

    std::array<uint8_t, k_block_size> block_;
    std::array<uint32_t, 4> state_{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    uint32_t length_hi_{0};
    uint32_t length_lo_{0};
    uint32_t head_{0};
    bool finished_{false};
};

} // namespace kb