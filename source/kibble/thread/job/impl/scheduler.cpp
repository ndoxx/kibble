#include "thread/job/impl/scheduler.h"
#include "assert/assert.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/worker.h"
#include "thread/job/job_system.h"

#include <algorithm>
#include <limits>

namespace kb
{
namespace th
{

Scheduler::Scheduler(JobSystem& js) : js_(js)
{
    std::fill(round_robin_.begin(), round_robin_.end(), 0);
}

void Scheduler::dispatch(Job* job)
{
    std::size_t& rr = round_robin_[js_.this_thread_id()];

    // The following code should be branchless (once optimized by the compiler)
    bool stealable = (job->meta.worker_affinity & (1 << k_stealable_bit)) >> k_stealable_bit;
    bool balance = (job->meta.worker_affinity & (1 << k_balance_bit)) >> k_balance_bit;
    uint32_t tid_hint = job->meta.worker_affinity & k_tid_hint_mask;

    // Sanity check
    K_ASSERT(tid_hint < js_.get_threads_count(), "Affinity TID hint bigger than workers count", nullptr)
        .watch(tid_hint)
        .watch(js_.get_threads_count());

    // Use TID hint strictly when balance is false, otherwise make sure that the TID produced is never lower than the
    // hint
    uint32_t tid = tid_hint + (balance * rr) % (uint32_t(js_.get_threads_count()) - tid_hint);
    // Advance round robin if balance is true
    rr = balance ? (rr + 1) % js_.get_threads_count() : rr;
    // Submit job to the appropriate queue
    js_.get_worker(tid).submit(job, stealable);
}

} // namespace th
} // namespace kb