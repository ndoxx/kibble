#pragma once
#include <algorithm>

namespace kb
{

/**
 * @brief Return the index of the minimal element in an iterable collection.
 *
 * @tparam InputIt Iterator type (deduced)
 * @param first Iterator pointing to the first element
 * @param last Iterator pointing past the last element
 * @return size_t Index of the minimal value
 */
template <class InputIt>
inline size_t min_index(InputIt first, InputIt last)
{
    return size_t(std::distance(first, std::min_element(first, last)));
}

/**
 * @brief Return the index of the maximal element in an iterable collection.
 *
 * @tparam InputIt Iterator type (deduced)
 * @param first Iterator pointing to the first element
 * @param last Iterator pointing past the last element
 * @return size_t Index of the maximal value
 */
template <class InputIt>
inline size_t max_index(InputIt first, InputIt last)
{
    return size_t(std::distance(first, std::max_element(first, last)));
}

} // namespace kb