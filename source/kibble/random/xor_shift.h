#pragma once

#include <cstdint>
#include <limits>
#include <ostream>

namespace kb
{
namespace rng
{

/**
 * @brief Implementation of a XorShift128+ random number generator.
 * This sub-type of Linear Feedback Shift Register generator is among the fastest non-cryptographically secure RNGs.
 * Most of the code is taken from: https://en.wikipedia.org/wiki/Xorshift
 *
 */
class XorShiftEngine
{
public:
    typedef uint64_t result_type; // Needed for compatibility with std distributions

    /**
     * @brief Internal engine state.
     *
     */
    struct Seed
    {
        uint64_t state_[2];

        Seed() = default;

        /**
         * @brief Construct a seed from a formatted string.
         * The state is 128 bits, so in order to implement it, two 64 bits variables are used, called the lower and
         * upper parts.
         *
         * @param str The string must be comprised of two numbers separated by a column. The lower and upper parts of
         * the state will be set to these numbers.
         */
        explicit Seed(const char *str);

        /**
         * @brief Construct a seed from a single number.
         * The lower and upper parts of the state will be initialized with two rounds of a splitmix64 algorithm.
         *
         * @param seed the single input seed number
         */
        explicit Seed(uint64_t seed);

        /**
         * @brief Construct a seed by directly initializing the lower and upper parts of the state.
         *
         * @param upper upper part of the seed
         * @param lower lower part of the seed
         *
         */
        constexpr Seed(uint64_t upper, uint64_t lower) : state_{upper, lower}
        {
        }

        /**
         * @brief Copy constructor.
         * Makes it possible to fork the RNG.
         *
         * @param rhs the other seed to copy
         *
         */
        constexpr Seed(const Seed &rhs) : state_{rhs.state_[0], rhs.state_[1]}
        {
        }

        /**
         * @brief Assignment operator.
         * Makes it possible to fork the RNG.
         *
         * @param rhs the other seed to copy
         * @return constexpr Seed&
         */
        constexpr Seed &operator=(const Seed &rhs)
        {
            state_[0] = rhs.state_[0];
            state_[1] = rhs.state_[1];
            return *this;
        }

        friend std::ostream &operator<<(std::ostream &stream, Seed rhs);
    };

    /**
     * @brief Default construction will seed with time.
     *
     */
    XorShiftEngine();

    /**
     * @brief Construct and seed.
     * 
     * @tparam SeedT Seed type
     * @param ss the seed
     */
    template <typename SeedT>
    XorShiftEngine(SeedT ss)
    {
        seed(ss);
    }

    /**
     * @brief Seed this generator with the current time.
     *
     */
    void seed();

    /**
     * @brief Set the seed of this generator.
     *
     * @param seed
     */
    inline void seed(Seed seed)
    {
        seed_ = seed;
    }

    /**
     * @brief Seed this generator using a single number.
     *
     * @param seed
     */
    inline void seed(uint64_t seed)
    {
        seed_ = Seed(seed);
    }

    /**
     * @brief Seed this generator using a formatted string.
     *
     * @param str The string must be comprised of two numbers separated by a column. The lower and upper parts of
     * the state will be set to these numbers.
     */
    inline void seed_string(const char *str)
    {
        seed_ = Seed(str);
    }

    /**
     * @brief Get the seed.
     *
     * @return Seed
     */
    inline Seed get_seed() const
    {
        return seed_;
    }

    /**
     * @brief Get a 64 bits unsigned random number.
     *
     * @return uint64_t
     */
    uint64_t rand64();

    /**
     * @brief Get a 64 bits unsigned random number.
     *
     * @return uint64_t
     */
    inline uint64_t operator()()
    {
        return rand64();
    }

    /**
     * @brief Get a 32 bits unsigned random number.
     * A 64 bits number is generated and cast to a 32 bits number. The last 32 bits will be truncated by the cast
     * operation.
     *
     * @return uint32_t
     */
    inline uint32_t rand()
    {
        return static_cast<uint32_t>(rand64());
    }

    /**
     * @brief Minimum numeric limit inherent to result_type (needed for compatibility with std distributions).
     *
     * @return uint64_t
     */
    inline uint64_t min() const
    {
        return std::numeric_limits<result_type>::min();
    }

    /**
     * @brief Maximum numeric limit inherent to result_type (needed for compatibility with std distributions).
     *
     * @return uint64_t
     */
    inline uint64_t max() const
    {
        return std::numeric_limits<result_type>::max();
    }

private:
    Seed seed_;
};

} // namespace rng
} // namespace kb