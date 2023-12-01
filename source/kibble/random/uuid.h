#pragma once

#include <cstdint>
#include <istream>
#include <memory>
#include <ostream>
#include <random>

/**
 * @brief UUIDv4 implementation
 *
 * This code is adapted from the uuid_v4 C++ library:
 * https://github.com/crashoz/uuid_v4
 * - under MIT licence (https://github.com/crashoz/uuid_v4/blob/master/LICENSE)
 * - main author is crashoz (https://github.com/crashoz)
 *
 * Modifications are:
 * - separated header / implementation
 * - changed naming convention
 * - hid intrinsics
 * - replaced C-style casts and implicit conversions
 * - minor changes to the interface
 * - documentation
 */

namespace kb
{

namespace UUIDv4
{
/**
 * @brief Represents a 128bits random UUIDv4 (RFC-4122 compliant)
 *
 */
class UUID
{
public:
    UUID() = default;

    /**
     * @brief Copy ctor
     *
     * @param other
     */
    UUID(const UUID& other);

    /**
     * @brief Builds a 128-bits UUID
     *
     * @param x upper 64 bits
     * @param y lower 64 bits
     */
    UUID(uint64_t x, uint64_t y);

    /**
     * @brief Builds an UUID from a bytes array
     *
     * @param bytes
     */
    explicit UUID(const uint8_t* bytes);

    /**
     * @brief Builds an UUID from a byte string (16 bytes long)
     *
     * @param bytes
     */
    explicit UUID(const std::string& bytes);

    /**
     * @brief Builds an UUID from raw data
     *
     * @param raw
     */
    explicit UUID(const char* raw);

    /**
     * @brief Static factory to parse an UUID from its string representation
     *
     * @param s
     * @return UUID
     */
    static UUID from_str_factory(const std::string& s);

    /**
     * @brief Static factory to parse an UUID from its string representation
     *
     * Raw pointer version
     *
     * @param s
     * @return UUID
     */
    static UUID from_str_factory(const char* raw);

    /**
     * @brief Static factory to build a UUID from random upper and lower bits
     *
     * This will also set the UUID version (4) and variant (1) fields
     *
     * @param upper
     * @param lower
     * @return UUID
     */
    static UUID from_upper_lower(uint64_t upper, uint64_t lower);

    /**
     * @brief Assignment operator
     *
     * @param other
     * @return UUID&
     */
    UUID& operator=(const UUID& other);

    friend bool operator==(const UUID& lhs, const UUID& rhs);
    friend bool operator<(const UUID& lhs, const UUID& rhs);
    friend bool operator!=(const UUID& lhs, const UUID& rhs);
    friend bool operator>(const UUID& lhs, const UUID& rhs);
    friend bool operator<=(const UUID& lhs, const UUID& rhs);
    friend bool operator>=(const UUID& lhs, const UUID& rhs);

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

    friend std::ostream& operator<<(std::ostream& stream, const UUID& uuid);

    friend std::istream& operator>>(std::istream& stream, UUID& uuid);

    inline std::size_t hash() const
    {
        return *(reinterpret_cast<const uint64_t*>(data_)) ^ *(reinterpret_cast<const uint64_t*>(data_ + 8));
    }

    inline uint8_t* data()
    {
        return data_;
    }

    inline const uint8_t* data() const
    {
        return data_;
    }

private:
    alignas(128) uint8_t data_[16];
};

template <typename RNG>
class UUIDGenerator
{
public:
    /**
     * @brief Initialize generator with a random seed
     *
     */
    UUIDGenerator()
        : generator_(std::random_device()()),
          distribution_(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max())
    {
    }

    /**
     * @brief Initialize generator with a set seed
     *
     * @param seed
     */
    UUIDGenerator(uint64_t seed)
        : generator_(seed),
          distribution_(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max())
    {
    }

    /**
     * @brief Initialize generator with an existing RNG instance
     *
     * @param gen
     */
    UUIDGenerator(RNG& gen)
        : generator_(gen), distribution_(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max())
    {
    }

    /**
     * @brief Generates a new UUID
     *
     * @return UUID
     */
    inline UUID get()
    {
        return UUID::from_upper_lower(distribution_(generator_), distribution_(generator_));
    }

    /**
     * @brief Generates a new UUID
     *
     * @return UUID
     */
    inline UUID operator()()
    {
        return get();
    }

    /// @brief Get the RNG instance
    inline RNG& get_generator()
    {
        return generator_;
    }

    /// @brief Get the RNG instance (const version)
    inline const RNG& get_generator() const
    {
        return generator_;
    }

private:
    RNG generator_;
    std::uniform_int_distribution<uint64_t> distribution_;
};

} // namespace UUIDv4
} // namespace kb

namespace std
{
template <>
struct hash<kb::UUIDv4::UUID>
{
    size_t operator()(const kb::UUIDv4::UUID& uuid) const
    {
        return uuid.hash();
    }
};
} // namespace std