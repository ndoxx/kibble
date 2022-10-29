#pragma once

#include <cstdint>
#include <istream>
#include <ostream>

/**
 * @brief UUIDv4 implementation
 *
 * This code is adapted from the uuid_v4 C++ library:
 * https://github.com/crashoz/uuid_v4
 * - under MIT licence (https://github.com/crashoz/uuid_v4/blob/master/LICENSE)
 * - main author is crashoz (https://github.com/crashoz)
 *
 * Modifications are:
 * - header / implementation separation
 * - naming convention
 * - hiding intrinsics
 * - replaced C-style casts and implicit conversions
 * - minor changes to the interface
 * - documentation
 */

namespace kb
{

/*
 * UUIDv4 (random 128-bits) RFC-4122
 */
class UUID
{
public:
    UUID() = default;

    UUID(const UUID &other);

    /* Builds a 128-bits UUID */
    UUID(uint64_t x, uint64_t y);

    UUID(const uint8_t *bytes);

    inline uint8_t *data()
    {
        return data_;
    }

    inline const uint8_t *data() const
    {
        return data_;
    }

    /* Builds an UUID from a byte string (16 bytes long) */
    explicit UUID(const std::string &bytes);

    /* Static factory to parse an UUID from its string representation */
    static UUID from_str_factory(const std::string &s);

    static UUID from_str_factory(const char *raw);

    void from_str(const char *raw);

    UUID &operator=(const UUID &other);

    friend bool operator==(const UUID &lhs, const UUID &rhs);
    friend bool operator<(const UUID &lhs, const UUID &rhs);
    friend bool operator!=(const UUID &lhs, const UUID &rhs);
    friend bool operator>(const UUID &lhs, const UUID &rhs);
    friend bool operator<=(const UUID &lhs, const UUID &rhs);
    friend bool operator>=(const UUID &lhs, const UUID &rhs);

    /**
     * @brief Serializes the uuid to a byte string (16 bytes)
     *
     * @return std::string
     */
    std::string bytes() const;

    /**
     * @brief Converts the uuid to its string representation
     *
     * @return std::string
     */
    std::string str() const;

    friend std::ostream &operator<<(std::ostream &stream, const UUID &uuid);

    friend std::istream &operator>>(std::istream &stream, UUID &uuid);

    inline std::size_t hash() const
    {
        return *(reinterpret_cast<const uint64_t *>(data_)) ^ *(reinterpret_cast<const uint64_t *>(data_) + 8);
    }

private:
    alignas(128) uint8_t data_[16];
};

} // namespace kb