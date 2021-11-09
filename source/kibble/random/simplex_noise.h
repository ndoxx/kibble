#pragma once

#include <algorithm>
#include <array>
#include <numeric>

namespace kb
{
namespace rng
{

/**
 * @brief An efficient coherent noise generator.
 * Contrary to Perlin noise which uses a square grid, this algorithm is based on a more subtle simplex tesselation of
 * space. This reduces the computational overhead in higher dimensions (O(n^2) versus O(n2^n)). The output also appears
 * more isotropic, with fewer directional artifacts. However, n-dimensional slices of a (n+1)-dimensional simplex noise
 * appear qualitatively different from n-dimensional simplex noise. The gradient of a simplex noise is quite smooth and
 * well-defined, however this class does not provide gradient computation functions at the moment.
 *
 * This is a rewrite of the code used in project WCore.
 * This code was itself a port from:
 * http://www.itn.liu.se/~stegu/simplexnoise/SimplexNoise.java
 *
 * The original description states:
 *    Based on example code by Stefan Gustavson (stegu@itn.liu.se).
 *    Optimisations by Peter Eastman (peastman@drizzle.stanford.edu).
 *    Better rank ordering method by Stefan Gustavson in 2012.
 *
 *    This could be speeded up even further, but it's useful as it is.
 *
 *    Version 2015-07-01
 *
 *    This code was placed in the public domain by its original author,
 *    Stefan Gustavson. You may use it as you see fit, but
 *    attribution is appreciated.
 *
 * The code retains the original comments.
 *
 */
class SimplexNoiseGenerator
{
public:
    /**
     * @brief Construct a new Simplex Noise Generator.
     * The generator is initialized with a default random engine, but the RNG can be changed thanks to the init()
     * function at any point in time.
     *
     */
    SimplexNoiseGenerator();

    /**
     * @brief (Re)initialize the random permutation table with another RNG.
     *
     * @tparam RandomEngineT type of random engine. The XorShiftEngine is advised.
     * @param rng random engine instance
     */
    template <typename RandomEngineT>
    void init(RandomEngineT &rng);

    /**
     * @brief Construct a new Simplex Noise Generator and initialize it with a given RNG instance.
     *
     * @tparam RandomEngineT type of random engine. The XorShiftEngine is advised.
     * @param rng random engine instance
     */
    template <typename RandomEngineT>
    SimplexNoiseGenerator(RandomEngineT &rng)
    {
        init(rng);
    }

    /**
     * @brief Produce 2D simplex noise at the input sample point.
     * 
     * @param xin the x coordinate
     * @param yin the y coordinate
     * @return float 
     */
    float operator()(float xin, float yin);

    /**
     * @brief Produce 3D simplex noise at the input sample point.
     *
     * @param xin the x coordinate
     * @param yin the y coordinate
     * @param zin the z coordinate
     * @return float
     */
    float operator()(float xin, float yin, float zin);

    /**
     * @brief Produce 4D simplex noise at the input sample point.
     *
     * @param xin the x coordinate
     * @param yin the y coordinate
     * @param zin the z coordinate
     * @param win the w coordinate
     * @return float
     */
    float operator()(float xin, float yin, float zin, float win);

private:
    std::array<uint8_t, 512> perm_;
};

template <typename RandomEngineT>
void SimplexNoiseGenerator::init(RandomEngineT &rng)
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