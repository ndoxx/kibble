#pragma once

#include "kibble/memory/util/alignment.h"
#include <atomic>

namespace kb::th
{

/**
 * @brief Allows to wait on a group of jobs to finish
 *
 */
class L1_ALIGN Barrier
{
public:
    Barrier() = default;

    /// @brief Called by the scheduler when a job using this barrier is scheduled
    inline void add_dependency() noexcept
    {
        pending_.fetch_add(1);
    }

    /// @brief Call when multiple jobs using this barrier are submitted
    inline void add_dependencies(std::size_t count) noexcept
    {
        pending_.fetch_add(count);
    }

    /// @brief Called by a worker thread when a job using this barrier is finished
    inline void remove_dependency() noexcept
    {
        pending_.fetch_sub(1);
    }

    /// @brief Non-blockingly check if all jobs using this barrier have finished
    inline bool finished() const noexcept
    {
        return pending_.load() == 0;
    }

    /// @brief Check if barrier is in use
    inline bool is_used() const noexcept
    {
        return in_use_.load();
    }

    /**
     * @brief Mark barrier as in use / not in use
     *
     * @param expected Expected state
     * @param desired Desired state
     * @return true if state was changed
     * @return false otherwise
     */
    inline bool mark_used(bool& expected, bool desired) noexcept
    {
        return in_use_.compare_exchange_strong(expected, desired);
    }

private:
    L1_ALIGN std::atomic<std::size_t> pending_{0};
    L1_ALIGN std::atomic<bool> in_use_{false};
};

} // namespace kb::th