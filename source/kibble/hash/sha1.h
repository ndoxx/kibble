#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace kb::hash
{

/**
* @brief Simple SHA-1 hash implementation.
*
* Adapted from Zlatko Michailov's implementation (based on Steve Reid's)
* - https://github.com/vog/sha1
* - Public domain (see implementation for modification history)
* 
*/
class SHA1
{
public:
    SHA1();
    void update(const std::string& s);
    void update(std::istream& is);

    /**
     * @brief Finalize the digest and return it as a 40-character lowercase hex string.
     *
     * Resets the object after the call so it can be reused.
     */
    std::string final();

    /**
     * @brief Finalize the digest and return the raw 20 bytes directly.
     *
     * Prefer this over final() when the binary digest is needed (e.g. as input
     * to base64_encode), since it avoids a hex-encode / hex-decode roundtrip.
     *
     * Resets the object after the call so it can be reused.
     */
    std::array<uint8_t, 20> final_bytes();

    static std::string from_file(const std::string& filename);

private:
    /**
     * @internal
     * @brief Apply padding, append the bit-length, and run the final transform(s).
     *
     * After this call digest_[] holds the five 32-bit words of the SHA-1 result.
     * The object is left in a reset state, ready for the next message.
     */
    void finalize();

private:
    uint32_t digest_[5];
    std::string buffer_;
    uint64_t transforms_;
};

} // namespace kb::hash