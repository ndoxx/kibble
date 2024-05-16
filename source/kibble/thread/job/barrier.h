#pragma once

#include "../../memory/util/alignment.h"
#include <atomic>
#include <limits>

namespace kb::th
{

using barrier_t = std::size_t;
[[maybe_unused]] static constexpr barrier_t k_no_barrier = std::numeric_limits<barrier_t>::max();

class L1_ALIGN Barrier
{
public:
    Barrier() = default;

    inline void add_dependencies(std::size_t count) noexcept
    {
        pending_.fetch_add(count);
    }

    inline void add_dependency() noexcept
    {
        pending_.fetch_add(1);
    }

    inline void remove_dependency() noexcept
    {
        pending_.fetch_sub(1);
    }

    inline bool finished() const noexcept
    {
        return pending_.load() == 0;
    }

    inline bool is_used() const noexcept
    {
        return in_use_.load();
    }

    inline bool is_used_exchange(bool& expected, bool desired) noexcept
    {
        return in_use_.compare_exchange_strong(expected, desired);
    }

private:
    L1_ALIGN std::atomic<std::size_t> pending_{0};
    L1_ALIGN std::atomic<bool> in_use_{false};
};

} // namespace kb::th