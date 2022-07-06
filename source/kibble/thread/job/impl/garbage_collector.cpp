#include "thread/job/impl/garbage_collector.h"
#include "logger/logger.h"
#include "memory/heap_area.h"
#include "memory/memory.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/worker.h"
#include "thread/job/job_system.h"

using namespace kb::memory;

namespace kb
{
namespace th
{

GarbageCollector::GarbageCollector(JobSystem &js) : js_(js)
{
}

void GarbageCollector::release(Job *job)
{
    delete_queue_.push(job);
}

void GarbageCollector::collect()
{
    // Check that we are on the main thread
    K_ASSERT(js_.this_thread_id() == 0, "Garbage collection must be performed on the main thread.");

    Job *job;
    while (delete_queue_.try_pop(job))
    {
#ifdef K_ENABLE_JOB_EXCEPTIONS
        auto p_except = job->p_except;
        auto label = job->meta.label;
#endif

        // Inform monitor about what happened with this job
        js_.get_monitor().report_job_execution(job->meta);

        // Return job to the pool
        if (!job->keep_alive)
        {
            K_DELETE(job, js_.get_shared_state().job_pool);
        }

#ifdef K_ENABLE_JOB_EXCEPTIONS
        // If (void) task threw an exception, rethrow, catch and log
        if (p_except != nullptr)
        {
            try
            {
                std::rethrow_exception(p_except);
            }
            catch (std::exception &e)
            {
                KLOGE("thread") << "Job #" << label << " threw an exception: " << e.what() << std::endl;
            }
        }
#endif
    }
}

} // namespace th
} // namespace kb