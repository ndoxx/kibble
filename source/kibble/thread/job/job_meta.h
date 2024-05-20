#pragma once

#include "logger2/channel.h"
#include <cstdint>
#include <string>

namespace kb::th
{

using tid_t = uint32_t;
using worker_affinity_t = uint32_t;

[[maybe_unused]] static constexpr uint32_t k_stealable_bit = 8;
[[maybe_unused]] static constexpr uint32_t k_balance_bit = 9;
[[maybe_unused]] static constexpr uint32_t k_tid_hint_mask = 0xff;

/**
 * @brief Encode worker affinity
 *
 * @param tid_hint Target worker ID, used strictly if balance is false, otherwise actual worker ID is never lower than
 * the hint
 * @param stealable If set to true, the job can be stolen by another thread
 * @param balance  If set to true, the scheduler will use a round robin to determine the actual worker ID
 * @return constexpr worker_affinity_t
 */
inline constexpr worker_affinity_t worker_affinity(uint32_t tid_hint, bool stealable, bool balance)
{
    return (tid_hint & k_tid_hint_mask) | uint32_t(stealable) << k_stealable_bit | uint32_t(balance) << k_balance_bit;
}

/**
 * @brief Encode a worker affinity that forces execution on a specific thread
 *
 * @param worker_id Target worker ID
 * @return constexpr worker_affinity_t
 */
inline constexpr worker_affinity_t force_worker(tid_t worker_id)
{
    return worker_affinity(worker_id, false, false);
}

/// A job with this affinity should be executed on the main thread
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_MAIN = worker_affinity(0, false, false);
/// A job with this affinity should be executed on any background thread but can be stolen by the main thread
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ASYNC = worker_affinity(1, true, true);
/// A job with this affinity should be executed on any background thread and cannot be stolen
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ASYNC_STRICT = worker_affinity(1, false, true);
/// A job with this affinity can be executed on any worker
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ANY = worker_affinity(0, true, true);

/**
 * @brief Metadata associated to a job.
 *
 */
struct JobMetadata
{
    JobMetadata() = default;
    JobMetadata(worker_affinity_t affinity, const std::string& profile_name);

    /// Workers this job can be pushed to
    worker_affinity_t worker_affinity = WORKER_AFFINITY_ANY;

    /// Descriptive name for the job (only used when profiling)
    std::string name;

private:
    friend class kb::log::Channel;
    bool essential__ = false;

public:
    inline bool is_essential() const
    {
        return essential__;
    }
};

} // namespace kb::th