#pragma once

#include <iterator>
#include <random>

namespace kb
{
namespace rng
{

template <typename Iter, typename RandomGenerator> Iter random_select(Iter start, Iter end, RandomGenerator& g)
{
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template <typename Iter> Iter random_select(Iter start, Iter end)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return random_select(start, end, gen);
}

} // namespace rng
} // namespace kb