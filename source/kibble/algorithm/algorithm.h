#pragma once
#include <algorithm>

namespace kb
{

template <class InputIt> inline size_t min_index(InputIt first, InputIt last)
{
    return size_t(std::distance(first, std::min_element(first, last)));
}

template <class InputIt> inline size_t max_index(InputIt first, InputIt last)
{
    return size_t(std::distance(first, std::max_element(first, last)));
}

} // namespace kb