#pragma once

#include "atomic_queue/atomic_queue.h"
#include "thread/job/config.h"

namespace kb
{
namespace th
{

struct Job;
class JobSystem;

/**
 * @internal
 * @brief This class allows concurrent job release requests.
 * All jobs marked for release are batch processed on a call to collect().
 * 
 */
class GarbageCollector
{
public:
    GarbageCollector(JobSystem &js);

    /**
     * @brief Mark this job for release.
     * It will be put in an atomic queue.
     * 
     * @param job The job to release.
     */
    void release(Job *job);

    /**
     * @brief Batch release all jobs in the queue.
     * @warning Must be called from the main thread.
     * 
     */
    void collect();

private:
    using DeleteQueue = atomic_queue::AtomicQueue<Job *, k_max_threads * k_max_jobs, nullptr, true, true, false, false>;

    JobSystem &js_;
    DeleteQueue delete_queue_;
};

} // namespace th
} // namespace kb