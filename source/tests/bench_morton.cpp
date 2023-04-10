// #include "math/easings.h"
#include "math/morton.h"
#include <benchmark/benchmark.h>
#include <random>


static void BM_morton_encode_2d(benchmark::State& state)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 31);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(kb::morton::encode(dist(rng), dist(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_morton_encode_2d);

static void BM_morton_decode_2d(benchmark::State& state)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 1023);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(kb::morton::decode_2d(dist(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_morton_decode_2d);

static void BM_morton_encode_3d(benchmark::State& state)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 31);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(kb::morton::encode(dist(rng), dist(rng), dist(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_morton_encode_3d);

static void BM_morton_decode_3d(benchmark::State& state)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 32767);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(kb::morton::decode_3d(dist(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_morton_decode_3d);

__attribute__((optnone)) uint64_t encode_linear(uint64_t x, uint64_t y)
{
    return y * 32 + x;
}

__attribute__((optnone)) uint64_t encode_linear(uint64_t x, uint64_t y, uint64_t z)
{
    return z * 1024 + y * 32 + x;
}

__attribute__((optnone)) std::tuple<uint64_t, uint64_t> decode_linear_2d(uint64_t key)
{
    return {key % 32, key / 32};
}

__attribute__((optnone)) std::tuple<uint64_t, uint64_t, uint64_t> decode_linear_3d(uint64_t key)
{
    uint64_t r = key % 1024;
    uint64_t y = key / 1024;
    uint64_t z = r / 32;
    uint64_t x = r % 32;
    return {x, y, z};
}

static void BM_linear_encode_2d(benchmark::State& state)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 31);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(encode_linear(dist(rng), dist(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_linear_encode_2d);

static void BM_linear_decode_2d(benchmark::State& state)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 1023);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(decode_linear_2d(dist(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_linear_decode_2d);

static void BM_linear_encode_3d(benchmark::State& state)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 31);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(encode_linear(dist(rng), dist(rng), dist(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_linear_encode_3d);

static void BM_linear_decode_3d(benchmark::State& state)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 32767);

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(decode_linear_3d(dist(rng)));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_linear_decode_3d);

BENCHMARK_MAIN();