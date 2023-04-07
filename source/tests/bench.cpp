// #include "math/easings.h"
#include <benchmark/benchmark.h>
#include <cmath>
#include <random>

#include "random/simplex_noise.h"
#include "random/xor_shift.h"

using namespace kb;
/*
static void BM_simplex_2d(benchmark::State& state)
{
    rng::XorShiftEngine rng;
    rng.seed(23456);
    rng::SimplexNoiseGenerator simplex(rng);

    for(auto _ : state)
    {
        benchmark::DoNotOptimize(simplex(0.256478f,-10.2646f));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_simplex_2d);

static void BM_simplex_3d(benchmark::State& state)
{
    rng::XorShiftEngine rng;
    rng.seed(23456);
    rng::SimplexNoiseGenerator simplex(rng);

    for(auto _ : state)
    {
        benchmark::DoNotOptimize(simplex(0.256478f,-10.2646f,12.87542f));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_simplex_3d);

static void BM_simplex_4d(benchmark::State& state)
{
    rng::XorShiftEngine rng;
    rng.seed(23456);
    rng::SimplexNoiseGenerator simplex(rng);

    for(auto _ : state)
    {
        benchmark::DoNotOptimize(simplex(0.256478f,-10.2646f,12.87542f,-45.18186f));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_simplex_4d);
*/

float Q_rsqrt(float y)
{
    float x2 = 0.5f * y;
    uint32_t i = *reinterpret_cast<uint32_t*>(&y);
    i = 0x5f3759df - (i >> 1);
    y = *reinterpret_cast<float*>(&i);
    y *= 1.5f - (x2 * y * y);
    // y *= 1.5f - (x2 * y * y);
    return y;
}

static void BM_qrsqrt(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(Q_rsqrt(42.f));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_qrsqrt);

static void BM_rsqrt(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(1.f / std::sqrt(42.f));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_rsqrt);

BENCHMARK_MAIN();