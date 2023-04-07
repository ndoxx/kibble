#pragma once

#include "thread/alignment.h"
#include "thread/job/config.h"
#include <array>
#include <cstdint>

namespace kb
{
namespace th
{

class JobSystem;
struct Job;

/**
 * @brief Dispatches the next job to the next worker in the line.
 * This simple static load balancing strategy ensures that a given worker is never given a job twice in a row, which
 * gives it some time to process its job queue before a new job is pushed.
 *
 * @note This scheduler is thread-safe.
 *
 */
class Scheduler
{
public:
    /**
     * @brief Construct a new Scheduler.
     *
     * @param js job system instance
     */
    Scheduler(JobSystem& js);

    /**
     * @brief Hand this job to the next worker.
     * If the job declares a non-default worker affinity in its metadata, the round robin will cycle until the
     * appropriate worker can handle it.
     *
     * @param job job instance
     */
    void dispatch(Job* job);

private:
    PAGE_ALIGN JobSystem& js_;
    // Each thread has its own round robin state (64b to avoid false sharing)
    PAGE_ALIGN std::array<std::size_t, k_max_threads> round_robin_;
};

} // namespace th
} // namespace kb