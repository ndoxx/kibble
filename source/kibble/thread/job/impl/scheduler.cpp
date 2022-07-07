#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/worker.h"
#include "thread/job/job_system.h"

#include <algorithm>
#include <limits>

namespace kb
{
namespace th
{

Scheduler::Scheduler(JobSystem &js) : js_(js)
{
    std::fill(round_robin_.begin(), round_robin_.end(), 0);
}

void Scheduler::dispatch(Job *job)
{
    std::size_t &rr = round_robin_[js_.this_thread_id()];

    // The following code should be branchless (once optimized by the compiler)
    // Avoid worker 0 if job needs async execution
    rr = (job->meta.worker_affinity == WORKER_AFFINITY_ASYNC) ? (rr + (rr == 0)) % js_.get_threads_count() : rr;
    // If job is to be executed on the main thread, force worker index to 0, and submit to the private queue
    size_t idx = (job->meta.worker_affinity == WORKER_AFFINITY_MAIN) ? 0 : rr;
    js_.get_worker(idx).submit(job, (job->meta.worker_affinity != WORKER_AFFINITY_MAIN));
    // Advance round robin
    rr = (rr + 1) % js_.get_threads_count();
}

} // namespace th
} // namespace kb