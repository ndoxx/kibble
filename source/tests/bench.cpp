#include "math/easings.h"
#include <random>
#include <benchmark/benchmark.h>

#define FUT__ inout_5

static void BM_EasingDirect(benchmark::State& state)
{
    std::random_device rd;
    std::default_random_engine rng(rd());
    std::uniform_real_distribution<float> dis(0.f, 1.f);

    for(auto _ : state)
    {
        benchmark::DoNotOptimize(kb::ease::FUT__(dis(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_EasingDirect);

static void BM_EasingLookup(benchmark::State& state)
{
    std::random_device rd;
    std::default_random_engine rng(rd());
    std::uniform_real_distribution<float> dis(0.f, 1.f);

    for(auto _ : state)
    {
        benchmark::DoNotOptimize(kb::experimental::ease::fast(kb::experimental::ease::Func::FUT__, dis(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_EasingLookup);

BENCHMARK_MAIN();