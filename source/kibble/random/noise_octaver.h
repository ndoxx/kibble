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

inline auto gen_octave(const std::array<float, 2>& dims, float frequency)
{
    return std::make_tuple(dims[0] * frequency, dims[1] * frequency);
}
inline auto gen_octave(const std::array<float, 3>& dims, float frequency)
{
    return std::make_tuple(dims[0] * frequency, dims[1] * frequency, dims[2] * frequency);
}
inline auto gen_octave(const std::array<float, 4>& dims, float frequency)
{
    return std::make_tuple(dims[0] * frequency, dims[1] * frequency, dims[2] * frequency, dims[3] * frequency);
}

} // namespace detail

template <typename NoiseGeneratorT> class NoiseOctaver
{
public:
    NoiseOctaver() = default;
    template <typename RandomEngineT> NoiseOctaver(RandomEngineT& rng) : gen_(rng) {}

    // Smooth filtering by local average
    // Kernel is 1/16 1/8 1/16
    //           1/8  1/4 1/8
    //           1/16 1/8 1/16
    float smooth_sample_2d(float x, float y, float dxy = 1.f)
    {
        float corners =
            (gen_(x - dxy, y - dxy) + gen_(x + dxy, y - dxy) + gen_(x - dxy, y + dxy) + gen_(x + dxy, y + dxy)) / 16;
        float sides = (gen_(x - dxy, y) + gen_(x + dxy, y) + gen_(x, y - dxy) + gen_(x, y + dxy)) / 8;
        float center = gen_(x, y) / 4;

        return corners + sides + center;
    }

    // Scaled sample
    inline float scaled_sample(float x, float y, float lb, float ub) { return rescale(gen_(x, y), lb, ub); }
    inline float scaled_sample(float x, float y, float z, float lb, float ub) { return rescale(gen_(x, y, z), lb, ub); }
    inline float scaled_sample(float x, float y, float z, float w, float lb, float ub)
    {
        return rescale(gen_(x, y, z, w), lb, ub);
    }

    // Octaved noise for pattern control
    template <size_t DIM>
    float octave(const std::array<float, DIM>& dims, size_t octaves, float frequency, float persistence)
    {
        float total = 0.f;
        float amplitude = 1.f;

        // We have to keep track of the largest possible amplitude,
        // because each octave adds more, and we need a value in [-1, 1].
        float max_amplitude = 0.f;

        for(size_t ii = 0; ii < octaves; ++ii)
        {
            total += amplitude * std::apply(gen_, detail::gen_octave(dims, frequency));
            frequency *= 1.95f; // Not set to 2.0 exactly, interference patterns are desired to break repetition
            max_amplitude += amplitude;
            amplitude *= persistence;
        }

        return total / max_amplitude;
    }
    inline float octave(float x, float y, size_t octaves, float frequency, float persistence)
    {
        return octave<2>({x, y}, octaves, frequency, persistence);
    }
    inline float octave(float x, float y, float z, size_t octaves, float frequency, float persistence)
    {
        return octave<3>({x, y, z}, octaves, frequency, persistence);
    }
    inline float octave(float x, float y, float z, float w, size_t octaves, float frequency, float persistence)
    {
        return octave<4>({x, y, z, w}, octaves, frequency, persistence);
    }
    inline float scaled_octave(float x, float y, size_t octaves, float frequency, float persistence, float lb, float ub)
    {
        return rescale(octave(x, y, octaves, frequency, persistence, lb, ub));
    }
    inline float scaled_octave(float x, float y, float z, size_t octaves, float frequency, float persistence, float lb,
                               float ub)
    {
        return rescale(octave(x, y, z, octaves, frequency, persistence, lb, ub));
    }
    inline float scaled_octave(float x, float y, float z, float w, size_t octaves, float frequency, float persistence,
                               float lb, float ub)
    {
        return rescale(octave(x, y, z, w, octaves, frequency, persistence, lb, ub));
    }
    inline float marble_x_2d(float x, float y, size_t octaves, float frequency, float persistence)
    {
        return std::cos(y * frequency + octave(x, y, octaves, frequency / 3, persistence));
    }
    inline float marble_y_2d(float x, float y, size_t octaves, float frequency, float persistence)
    {
        return std::cos(x * frequency + octave(x, y, octaves, frequency / 3, persistence));
    }

private:
    inline float rescale(float x, float lb, float ub) { return x * (ub - lb) / 2 + (ub + lb) / 2; }

private:
    NoiseGeneratorT gen_;
};

} // namespace rng
} // namespace kb