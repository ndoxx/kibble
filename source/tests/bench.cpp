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
        kb::easing::FUT__(dis(rng));
}
BENCHMARK(BM_EasingDirect);

static void BM_EasingLookup(benchmark::State& state)
{
    std::random_device rd;
    std::default_random_engine rng(rd());
    std::uniform_real_distribution<float> dis(0.f, 1.f);

    for(auto _ : state)
        kb::experimental::easing::fast(kb::experimental::easing::Func::FUT__, dis(rng));
}
BENCHMARK(BM_EasingLookup);

BENCHMARK_MAIN();