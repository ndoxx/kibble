#pragma once

#include <iterator>
#include <random>

namespace kb
{
namespace rng
{

/**
 * @brief Select an element at random in a collection.
 *
 * @tparam Iter iterator type
 * @tparam RandomGenerator RNG to use
 * @param start iterator to the first element
 * @param end iterator past the last element
 * @param g random generator instance
 * @return an iterator to the randomly selected element
 */
template <typename Iter, typename RandomGenerator>
Iter random_select(Iter start, Iter end, RandomGenerator &g)
{
    std::uniform_int_distribution<long> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

/**
 * @brief Select an element at random in a collection.
 * A default RNG is used.
 *
 * @tparam Iter iterator type
 * @param start iterator to the first element
 * @param end iterator past the last element
 * @return an iterator to the randomly selected element
 */
template <typename Iter>
Iter random_select(Iter start, Iter end)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return random_select(start, end, gen);
}

} // namespace rng
} // namespace kb