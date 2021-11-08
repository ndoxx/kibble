#pragma once

#include <array>
#include <cmath>
#include <tuple>

namespace kb
{
namespace rng
{
namespace detail
{
/**
 * @internal
 * @brief Helper function to generate a 2D noise octave
 *
 * @param coords 2D coordinates
 * @param frequency octave frequency
 * @return auto
 */
inline auto gen_octave(const std::array<float, 2> &coords, float frequency)
{
    return std::make_tuple(coords[0] * frequency, coords[1] * frequency);
}

/**
 * @internal
 * @brief Helper function to generate a 3D noise octave
 *
 * @param coords 3D coordinates
 * @param frequency octave frequency
 * @return auto
 */
inline auto gen_octave(const std::array<float, 3> &coords, float frequency)
{
    return std::make_tuple(coords[0] * frequency, coords[1] * frequency, coords[2] * frequency);
}

/**
 * @internal
 * @brief Helper function to generate a 4D noise octave
 *
 * @param coords 4D coordinates
 * @param frequency octave frequency
 * @return auto
 */
inline auto gen_octave(const std::array<float, 4> &coords, float frequency)
{
    return std::make_tuple(coords[0] * frequency, coords[1] * frequency, coords[2] * frequency, coords[3] * frequency);
}

} // namespace detail

/**
 * @brief Combine multiple samples of a coherent noise into a rich spectral mix.
 * - The octave noise combines scaled coherent noise at various frequencies so the output looks less regular and more
 * organic. This type of generator is well suited for terrain or cloud procedural generation for instance, where local
 * height or density appear to vary in space with different levels of detail.
 * - The noise smoother produces a smoothed out coherent noise thanks to multisampling and kernel convolution.
 * - The marble noise functions create a non-isotropic noise with band-like artifacts, well suited for wood or marble
 * texture generation.
 *
 * @tparam NoiseGeneratorT Coherent noise generator type, like SimplexNoiseGenerator
 */
template <typename NoiseGeneratorT>
class NoiseBlender
{
public:
    NoiseBlender() = default;

    /**
     * @brief Construct a new Noise Blender object.
     * This constructor initializes the underlying coherent noise generator with a given RNG.
     *
     * @tparam RandomEngineT RNG type
     * @param rng RNG instance
     */
    template <typename RandomEngineT>
    NoiseBlender(RandomEngineT &rng) : gen_(rng)
    {
    }

    /**
     * @brief Performs smooth filtering by local average.
     * Multi-sample the coherent noise source and convolute with a kernel.
     * Kernel is 1/16 1/8 1/16
     *           1/8  1/4 1/8
     *           1/16 1/8 1/16
     *
     * @todo Refactor to accept a kernel object.
     * @param x the x coordinate
     * @param y the y coordinate
     * @param dxy multisampling offset in the x and y directions
     * @return float
     */
    float smooth_sample_2d(float x, float y, float dxy = 1.f)
    {
        float corners =
            (gen_(x - dxy, y - dxy) + gen_(x + dxy, y - dxy) + gen_(x - dxy, y + dxy) + gen_(x + dxy, y + dxy)) / 16;
        float sides = (gen_(x - dxy, y) + gen_(x + dxy, y) + gen_(x, y - dxy) + gen_(x, y + dxy)) / 8;
        float center = gen_(x, y) / 4;

        return corners + sides + center;
    }

    /**
     * @brief Rescale a 2D noise sample.
     * The output is guaranteed to lie between the lower and upper bounds.
     *
     * @param x the x coordinate
     * @param y the y coordinate
     * @param lb the lower bound
     * @param ub the upper bound
     * @return float
     */
    inline float scaled_sample(float x, float y, float lb, float ub)
    {
        return rescale(gen_(x, y), lb, ub);
    }

    /**
     * @brief Rescale a 3D noise sample.
     * The output is guaranteed to lie between the lower and upper bounds.
     *
     * @param x the x coordinate
     * @param y the y coordinate
     * @param z the z coordinate
     * @param lb the lower bound
     * @param ub the upper bound
     * @return float
     */
    inline float scaled_sample(float x, float y, float z, float lb, float ub)
    {
        return rescale(gen_(x, y, z), lb, ub);
    }

    /**
     * @brief Rescale a 4D noise sample.
     * The output is guaranteed to lie between the lower and upper bounds.
     *
     * @param x the x coordinate
     * @param y the y coordinate
     * @param z the z coordinate
     * @param w the w coordinate
     * @param lb the lower bound
     * @param ub the upper bound
     * @return float
     */
    inline float scaled_sample(float x, float y, float z, float w, float lb, float ub)
    {
        return rescale(gen_(x, y, z, w), lb, ub);
    }

    /**
     * @brief Produce octave noise in 2, 3 or 4 dimensions.
     * Scaled coherent noise samples at various frequencies are combined to produce a noise output with various levels
     * of detail.\n
     * Noise frequency will be almost doubled for each octave. It is not exactly doubled so as to break repetition. Each
     * octave's amplitude is in a geometric progression controlled by a persistence parameter, so the effect of higher
     * frequencies can be modulated.
     *
     * @tparam DIM dimension
     * @param coords coordinates
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @return float
     */
    template <size_t DIM>
    float octave(const std::array<float, DIM> &coords, size_t octaves, float frequency, float persistence)
    {
        float total = 0.f;
        float amplitude = 1.f;

        // We have to keep track of the largest possible amplitude,
        // because each octave adds more, and we need a value in [-1, 1].
        float max_amplitude = 0.f;

        for (size_t ii = 0; ii < octaves; ++ii)
        {
            total += amplitude * std::apply(gen_, detail::gen_octave(coords, frequency));
            frequency *= 1.95f; // Not set to 2.0 exactly, interference patterns are desired to break repetition
            max_amplitude += amplitude;
            amplitude *= persistence;
        }

        return total / max_amplitude;
    }

    /**
     * @brief Produce 2D octave noise.
     *
     * @see octave(const std::array<float, DIM> &, size_t, float, float)
     * @param x the x coordinate
     * @param y the y coordinate
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @return float
     */
    inline float octave(float x, float y, size_t octaves, float frequency, float persistence)
    {
        return octave<2>({x, y}, octaves, frequency, persistence);
    }

    /**
     * @brief Produce 3D octave noise.
     *
     * @see octave(const std::array<float, DIM> &, size_t, float, float)
     * @param x the x coordinate
     * @param y the y coordinate
     * @param z the z coordinate
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @return float
     */
    inline float octave(float x, float y, float z, size_t octaves, float frequency, float persistence)
    {
        return octave<3>({x, y, z}, octaves, frequency, persistence);
    }

    /**
     * @brief Produce 4D octave noise.
     *
     * @see octave(const std::array<float, DIM> &, size_t, float, float)
     * @param x the x coordinate
     * @param y the y coordinate
     * @param z the z coordinate
     * @param w the w coordinate
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @return float
     */
    inline float octave(float x, float y, float z, float w, size_t octaves, float frequency, float persistence)
    {
        return octave<4>({x, y, z, w}, octaves, frequency, persistence);
    }

    /**
     * @brief Produce a rescaled 2D octave noise.
     *
     * @param x the x coordinate
     * @param y the y coordinate
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @param lb scale lower bound
     * @param ub scale upper bound
     * @return float
     */
    inline float scaled_octave(float x, float y, size_t octaves, float frequency, float persistence, float lb, float ub)
    {
        return rescale(octave(x, y, octaves, frequency, persistence), lb, ub);
    }

    /**
     * @brief Produce a rescaled 3D octave noise.
     *
     * @param x the x coordinate
     * @param y the y coordinate
     * @param z the z coordinate
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @param lb scale lower bound
     * @param ub scale upper bound
     * @return float
     */
    inline float scaled_octave(float x, float y, float z, size_t octaves, float frequency, float persistence, float lb,
                               float ub)
    {
        return rescale(octave(x, y, z, octaves, frequency, persistence), lb, ub);
    }

    /**
     * @brief Produce a rescaled 4D octave noise.
     *
     * @param x the x coordinate
     * @param y the y coordinate
     * @param z the z coordinate
     * @param w the w coordinate
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @param lb scale lower bound
     * @param ub scale upper bound
     * @return float
     */
    inline float scaled_octave(float x, float y, float z, float w, size_t octaves, float frequency, float persistence,
                               float lb, float ub)
    {
        return rescale(octave(x, y, z, w, octaves, frequency, persistence), lb, ub);
    }

    /**
     * @brief Produce 2D horizontal marble noise.
     * The output noise will present band-like features extending in the x direction.
     *
     * @param x the x coordinate
     * @param y the y coordinate
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @return float
     */
    inline float marble_x_2d(float x, float y, size_t octaves, float frequency, float persistence)
    {
        return std::cos(y * frequency + octave(x, y, octaves, frequency / 3, persistence));
    }

    /**
     * @brief Produce 2D vertical marble noise.
     * The output noise will present band-like features extending in the y direction.
     *
     * @param x the x coordinate
     * @param y the y coordinate
     * @param octaves number of frequency octaves
     * @param frequency initial frequency, will be (almost) doubled each octave
     * @param persistence amplitude common ratio
     * @return float
     */
    inline float marble_y_2d(float x, float y, size_t octaves, float frequency, float persistence)
    {
        return std::cos(x * frequency + octave(x, y, octaves, frequency / 3, persistence));
    }

private:
    /**
     * @internal
     * @brief Rescale a noise sample x in [0,1] to [lb, ub].
     *
     * @param x input noise sample
     * @param lb the lower bound
     * @param ub the upper bound
     * @return float
     */
    inline float rescale(float x, float lb, float ub)
    {
        return x * (ub - lb) / 2 + (ub + lb) / 2;
    }

private:
    NoiseGeneratorT gen_;
};

} // namespace rng
} // namespace kb