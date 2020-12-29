#pragma once

#include <algorithm>
#include <array>
#include <numeric>

namespace kb
{
namespace rng
{

class SimplexNoiseGenerator
{
public:
    SimplexNoiseGenerator();

    template <typename RandomEngineT> void init(RandomEngineT& rng);
    template <typename RandomEngineT> SimplexNoiseGenerator(RandomEngineT& rng) { init(rng); }

    float operator()(float xin, float yin);
    float operator()(float xin, float yin, float zin);
    float operator()(float xin, float yin, float zin, float win);

private:
    std::array<uint8_t, 512> perm_;
};

template <typename RandomEngineT> void SimplexNoiseGenerator::init(RandomEngineT& rng)
{
    // Initialize permutation table
    std::array<uint8_t, 256> rand_perm;
    std::iota(rand_perm.begin(), rand_perm.end(), 0);
    std::shuffle(rand_perm.begin(), rand_perm.end(), rng);
    std::copy(rand_perm.begin(), rand_perm.end(), perm_.begin());
    std::copy(rand_perm.begin(), rand_perm.end(), perm_.begin() + 256);
}

} // namespace rng
} // namespace kb