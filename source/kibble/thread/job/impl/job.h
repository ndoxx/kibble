#pragma once

#include "config.h"
#include "kibble/thread/job/barrier_id.h"
#include "kibble/thread/job/impl/job_graph.h"
#include "kibble/thread/job/job_meta.h"

#include <functional>

namespace kb::th
{

/**
 * @brief Represents some amount of work to execute.
 *
 */
struct L1_ALIGN Job : public ProcessNode<Job*, KIBBLE_JOBSYS_MAX_PARENT_JOBS, KIBBLE_JOBSYS_MAX_CHILD_JOBS>
{
    /// Job metadata
    JobMetadata meta;
    /// The function to execute
    std::function<void(void)> kernel = []() {};
    /// If true, job will not be returned to the pool once finished
    bool keep_alive = false;
    /// Barrier ID for this job and its dependents
    barrier_t barrier_id{k_no_barrier};
};

} // namespace kb::th