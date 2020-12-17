#pragma once

#include <cstdint>
#include <limits>
#include <ostream>

namespace kb
{
namespace rng
{
/*
    Implementation of a XorShift128+ random number generator.
    Most of the code is taken from: https://en.wikipedia.org/wiki/Xorshift
*/
class XorShiftEngine
{
public:
    typedef uint64_t result_type; // Needed for compatibility with std distributions

    // Internal engine state
    struct Seed
    {
        uint64_t state_[2];

        Seed() = default;
        explicit Seed(const char* str);
        explicit Seed(uint64_t seed);
        constexpr Seed(uint64_t upper, uint64_t lower) : state_{upper, lower} {}
        constexpr Seed(const Seed& rhs) : state_{rhs.state_[0], rhs.state_[1]} {}
        constexpr Seed& operator=(const Seed& rhs)
        {
            state_[0] = rhs.state_[0];
            state_[1] = rhs.state_[1];
            return *this;
        }

        friend std::ostream& operator<<(std::ostream& stream, Seed rhs);
    };

    // Default construction will seed with time
    XorShiftEngine();

    // Seed with current time
    void seed();
    inline void seed(Seed seed) { seed_ = seed; }
    inline void seed(uint64_t seed) { seed_ = Seed(seed); }
    inline void seed_string(const char* str) { seed_ = Seed(str); }
    inline Seed get_seed() const { return seed_; }

    // Get 64bits unsigned random number
    uint64_t rand64();
    inline uint64_t operator()() { return rand64(); }
    // Get 32bits unsigned random number
    inline uint32_t rand() { return static_cast<uint32_t>(rand64()); }

    // Numeric limits inherent to result_type (needed for compatibility with std distributions)
    inline uint64_t min() const { return std::numeric_limits<result_type>::min(); }
    inline uint64_t max() const { return std::numeric_limits<result_type>::max(); }

private:
    Seed seed_;
};

} // namespace rng
} // namespace kb