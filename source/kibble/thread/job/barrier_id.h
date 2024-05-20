#pragma once

#include <cstddef>
#include <limits>

namespace kb::th
{

/// @brief Barrier ID type
using barrier_t = std::size_t;
/// @brief Barrier ID value representing no barrier
[[maybe_unused]] static constexpr barrier_t k_no_barrier = std::numeric_limits<barrier_t>::max();

} // namespace kb::th