#include <benchmark/benchmark.h>
#include <cmath>


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
