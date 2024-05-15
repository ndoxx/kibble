#pragma once

#include "../../memory/util/alignment.h"
#include <atomic>

namespace kb::th
{

class L1_ALIGN Barrier
{
public:
    Barrier() = default;

    inline void add_job()
    {
        pending_.fetch_add(1, std::memory_order_relaxed);
    }

    inline void remove_job()
    {
        pending_.fetch_sub(1, std::memory_order_relaxed);
    }

    inline bool finished() const
    {
        return pending_.load() == 0;
    }

private:
    std::atomic<std::size_t> pending_{0};
};

} // namespace kb::th