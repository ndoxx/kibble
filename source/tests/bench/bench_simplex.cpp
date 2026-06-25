// #include "math/easings.h"
#include <benchmark/benchmark.h>
#include <cmath>
#include <random>

#include "kibble/random/simplex_noise.h"
#include "kibble/random/xor_shift.h"

using namespace kb;

static void BM_simplex_2d(benchmark::State& state)
{
    rng::XorShiftEngine rng;
    rng.seed(23456);
    rng::SimplexNoiseGenerator simplex(rng);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(simplex(0.256478f, -10.2646f));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_simplex_2d);

static void BM_simplex_3d(benchmark::State& state)
{
    rng::XorShiftEngine rng;
    rng.seed(23456);
    rng::SimplexNoiseGenerator simplex(rng);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(simplex(0.256478f, -10.2646f, 12.87542f));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_simplex_3d);

static void BM_simplex_4d(benchmark::State& state)
{
    rng::XorShiftEngine rng;
    rng.seed(23456);
    rng::SimplexNoiseGenerator simplex(rng);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(simplex(0.256478f, -10.2646f, 12.87542f, -45.18186f));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_simplex_4d);

BENCHMARK_MAIN();