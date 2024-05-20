#include "thread/job/job_system.h"
#include "assert/assert.h"
#include "config.h"
#include "fmt/core.h"
#include "logger2/logger.h"
#include "math/constexpr_math.h"
#include "memory/allocator/atomic_pool_allocator.h"
#include "memory/allocator/linear_allocator.h"
#include "memory/arena.h"
#include "memory/heap_area.h"
#include "thread/job/impl/barrier.h"
#include "thread/job/impl/job.h"
#include "thread/job/impl/job_graph.h"
#include "thread/job/impl/monitor.h"
#include "thread/job/impl/scheduler.h"
#include "thread/job/impl/worker.h"
#include "time/instrumentation.h"

#include "fmt/std.h"
#include "util/internal.h"
#include <stack>
#include <thread>
#include <unordered_set>

namespace kb
{

namespace th
{

inline size_t worker_count(const JobSystem::Config& scheme, size_t CPU_cores)
{
    size_t max_threads = (scheme.max_workers != 0) ? std::min(KIBBLE_JOBSYS_MAX_THREADS, scheme.max_workers + 1)
                                                   : KIBBLE_JOBSYS_MAX_THREADS;
    return std::min(max_threads, CPU_cores);
}

inline size_t local_arena_requirements(const JobSystem::Config& scheme)
{
    // Allocation size for barriers
    size_t barrier_alloc_size = sizeof(Barrier) * scheme.max_barriers + kb::memory::k_cache_line_size;

    // Allocation size for workers
    size_t workers = worker_count(scheme, std::thread::hardware_concurrency());
    // Workers are always aligned to the cache line, so we want the nearest multiple of alignment
    constexpr size_t worker_size = math::round_up_pow2(sizeof(WorkerThread), kb::memory::k_cache_line_size);
    size_t worker_alloc_size = workers * worker_size;

    // Allocation size for shared state
    constexpr size_t shared_state_size = sizeof(SharedState) + kb::memory::k_cache_line_size;

    return barrier_alloc_size + worker_alloc_size + shared_state_size + kb::memory::k_cache_line_size;
}

struct JobSystem::Internal
{
    using JobSystemArena =
        memory::MemoryArena<memory::LinearAllocator, memory::policy::SingleThread, memory::policy::NoBoundsChecking,
                            memory::policy::NoMemoryTagging, memory::policy::NoMemoryTracking>;

    using JobPoolArena =
        memory::MemoryArena<memory::AtomicPoolAllocator<KIBBLE_JOBSYS_JOB_QUEUE_SIZE * KIBBLE_JOBSYS_MAX_THREADS>,
                            memory::policy::SingleThread, memory::policy::NoBoundsChecking,
                            memory::policy::NoMemoryTagging, memory::policy::NoMemoryTracking>;

    Internal(JobSystem* js, memory::HeapArea& area)
        : arena("JobSystemLocalArena", area, local_arena_requirements(js->get_config())),
          job_pool("JobPool", area, sizeof(Job), kb::memory::k_cache_line_size), scheduler(*js), monitor(*js)
    {
    }

    L1_ALIGN JobSystemArena arena;
    L1_ALIGN JobPoolArena job_pool;

    Scheduler scheduler;
    Monitor monitor;
};
} // namespace th

namespace internal_deleter
{
template <>
void Deleter<th::JobSystem::Internal>::operator()(th::JobSystem::Internal* p)
{
    delete p;
}
} // namespace internal_deleter

namespace th
{

JobMetadata::JobMetadata(worker_affinity_t affinity, const std::string& profile_name) : worker_affinity(affinity)
{
    name = profile_name;
}

size_t JobSystem::get_memory_requirements(const Config& scheme)
{
    // Total size of a Job node inside the pool
    // Jobs are always aligned to the cache line, so we want the nearest multiple of alignment
    // and we also have to take into account additional data written by the allocator
    constexpr size_t k_job_node_size =
        math::round_up_pow2(sizeof(Job) + Internal::JobPoolArena::k_allocation_overhead, kb::memory::k_cache_line_size);

    // Area will need enough space for each job
    // Also add a cache line, to account for heap area block alignment
    size_t job_alloc_size =
        KIBBLE_JOBSYS_JOB_QUEUE_SIZE * KIBBLE_JOBSYS_MAX_THREADS * k_job_node_size + kb::memory::k_cache_line_size;

    return local_arena_requirements(scheme) + job_alloc_size;
}

JobSystem::JobSystem(memory::HeapArea& area, const Config& scheme, const kb::log::Channel* log_channel)
    : config_(scheme), internal_(make_internal<Internal>(this, area)), log_channel_(log_channel)
{
    klog(log_channel_).uid("JobSystem").info("Initializing.");

    // * Allocate barriers
    barriers_ = K_NEW_ARRAY_DYNAMIC(Barrier, config_.max_barriers, internal_->arena);

    // * Allocate shared state
    shared_state_ = K_NEW(SharedState, internal_->arena);

    // * Allocate workers
    // Find the number of CPU cores
    CPU_cores_count_ = std::thread::hardware_concurrency();
    // Select worker count based on scheme and CPU cores
    threads_count_ = worker_count(scheme, CPU_cores_count_);

    klog(log_channel_).uid("JobSystem").debug("Detected {} CPU cores.", CPU_cores_count_);
    klog(log_channel_).uid("JobSystem").debug("Spawning {} (async) worker threads.", threads_count_ - 1);

    if (threads_count_ == 1)
    {
        klog(log_channel_)
            .uid("JobSystem")
            .warn("Tasks marked with WORKER_AFFINITY_ASYNC will be scheduled to the main thread.");
    }

    workers_ = K_NEW_ARRAY_DYNAMIC(WorkerThread, threads_count_, internal_->arena);

    // * Spawn workers
    for (uint32_t ii = 0; ii < threads_count_; ++ii)
    {
        WorkerProperties props;
        props.max_stealing_attempts = scheme.max_stealing_attempts;
        props.tid = ii;

        auto& worker = workers_[ii];
        worker.spawn(this, shared_state_, props);
        auto native_id = worker.is_background() ? worker.get_native_thread_id() : std::this_thread::get_id();
        thread_ids_.insert({native_id, worker.get_tid()});
        klog(log_channel_)
            .uid("JobSystem")
            .verbose("Spawned worker #{}, native thread id: {}", worker.get_tid(), native_id);
    }

    klog(log_channel_).uid("JobSystem").debug("Ready.");
}

JobSystem::~JobSystem()
{
    // * Shutdown thread pool
    shutdown();

    // * Destroy workers
    K_DELETE_ARRAY(workers_, internal_->arena);

    // * Destroy shared state
    K_DELETE(shared_state_, internal_->arena);

    // * Destroy barriers
    for (barrier_t id = 0; id < config_.max_barriers; ++id)
    {
        K_ASSERT(!barriers_[id].is_used(), "Barrier still in use.", log_channel_).watch_var__(id, "Barrier index");
    }

    K_DELETE_ARRAY(barriers_, internal_->arena);
}

void JobSystem::shutdown()
{
    klog(log_channel_).uid("JobSystem").info("Shutting down.");
    klog(log_channel_).uid("JobSystem").debug("Waiting for jobs to finish.");
    wait();

    // Notify all threads they are going to die
    shared_state_->running.store(false, std::memory_order_release);
    shared_state_->cv_wake.notify_all();
    for (size_t idx = 0; idx < threads_count_; ++idx)
    {
        workers_[idx].join();
    }

    // We just killed all threads, including the logger thread
    // So we must go back to sync mode
    kb::log::Channel::set_async(nullptr);
    klog(log_channel_).uid("JobSystem").debug("All threads have joined.");

#ifdef K_USE_JOB_SYSTEM_PROFILING
    // Log worker statistics
    internal_->monitor.update_statistics();
    klog(log_channel_).uid("JobSystem").verbose("Thread statistics:");
    for (size_t idx = 0; idx < threads_count_; ++idx)
    {
        internal_->monitor.log_statistics(workers_[idx].get_tid(), log_channel_);
    }
#endif

    klog(log_channel_).uid("JobSystem").info("Shutdown complete.");
}

barrier_t JobSystem::create_barrier()
{
    // Find first unused barrier
    for (barrier_t id = 0; id < config_.max_barriers; ++id)
    {
        bool expected{false};
        if (barriers_[id].mark_used(expected, true))
        {
            return id;
        }
    }

    // No unused barrier found
    return k_no_barrier;
}

void JobSystem::destroy_barrier(barrier_t id)
{
    K_ASSERT(id < config_.max_barriers, "Barrier index out of bounds.", log_channel_);
    Barrier& barrier = barriers_[id];
    // Check that barrier has no pending jobs
    K_ASSERT(barrier.finished(), "Tried to destroy barrier with unfinished jobs.", log_channel_);
    // Mark barrier as unused
    bool expected{true};
    barrier.mark_used(expected, false);
    K_ASSERT(expected, "Tried to destroy unused barrier.", log_channel_);
}

Barrier& JobSystem::get_barrier(barrier_t id)
{
    K_ASSERT(id < config_.max_barriers, "Barrier index out of bounds.", log_channel_);
    return barriers_[id];
}

Job* JobSystem::create_job(std::function<void()>&& kernel, JobMetadata&& meta)
{
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());

    Job* job = K_NEW(Job, internal_->job_pool);
    job->kernel = std::move(kernel);
    job->meta = std::move(meta);
    return job;
}

void JobSystem::release_job(Job* job)
{
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());

    // Make sure that the job was processed
    K_ASSERT(job->check_state(JobState::Processed), "Tried to release unprocessed job.", log_channel_);

    // Return job to the pool
    K_DELETE(job, internal_->job_pool);
}

bool JobSystem::try_schedule(Job* job, size_t num_jobs)
{
    JS_PROFILE_FUNCTION(instrumentor_, this_thread_id());
    // Sanity check
    K_ASSERT(job->is_ready(), "Tried to schedule job with unfinished dependencies.", log_channel_);

    JobState expected = JobState::Idle;
    if (job->exchange_state(expected, JobState::Pending))
    {
        // Dspatch and wake up workers
        if (num_jobs != 0)
        {
            shared_state_->pending.fetch_add(num_jobs, std::memory_order_release);
        }
        internal_->scheduler.dispatch(job);
        shared_state_->cv_wake.notify_all();
        return true;
    }

    return false;
}

// Main thread and workers (on rescheduling) atomically increment pending each
// time a job is pushed to the queue.
// Main thread and workers atomically decrement pending each time they finished a job.
// Then we just need to wait for pending to return to zero in order
// to be sure all worker threads have finished.
bool JobSystem::is_busy() const
{
    return shared_state_->pending.load(std::memory_order_acquire) > 0;
}

// NOTE(ndx): Instead of busy-waiting I tried
//      std::unique_lock<std::mutex> lock(shared_state_->wait_mutex);
//      shared_state_->cv_wait.wait(lock, [this]() { return !is_busy(); });
// and in WorkerThread::execute() I do this after the fetch_sub:
//      ss_.cv_wait.notify_one();
// But it deadlocks (lost wakeups?)
void JobSystem::wait_until(std::function<bool()> condition)
{
    // Do some work to assist worker threads
#ifdef K_USE_JOB_SYSTEM_PROFILING
    int64_t idle_time_us = 0;
#endif

    while (condition())
    {
        if (!workers_[0].foreground_work())
        {
            // There's nothing we can do, just wait. Some work may come to us.
#ifdef K_USE_JOB_SYSTEM_PROFILING
            microClock clk;
#endif
            shared_state_->cv_wake.notify_all(); // wake worker threads
            std::this_thread::yield();           // allow this thread to be rescheduled
#ifdef K_USE_JOB_SYSTEM_PROFILING
            idle_time_us += clk.get_elapsed_time().count();
#endif
        }
    }

#ifdef K_USE_JOB_SYSTEM_PROFILING
    auto& activity = workers_[0].get_activity();
    activity.idle_time_us += idle_time_us;
    internal_->monitor.report_thread_activity(activity);
    activity.reset();
    internal_->monitor.update_statistics();
#endif
}

void JobSystem::wait(std::function<bool()> condition)
{
    JS_PROFILE_SCOPE(instrumentor_, "JobSystem::wait", this_thread_id());
    wait_until([this, &condition]() { return is_busy() && condition(); });
}

void JobSystem::wait_on_barrier(uint64_t barrier_id)
{
    JS_PROFILE_SCOPE(instrumentor_, "JobSystem::wait_on_barrier", this_thread_id());
    const auto& barrier = get_barrier(barrier_id);
    wait_until([&barrier]() { return !barrier.finished(); });
}

WorkerThread& JobSystem::get_worker(size_t idx)
{
    return workers_[idx];
}

/// Get the worker at input index (const).
const WorkerThread& JobSystem::get_worker(size_t idx) const
{
    return workers_[idx];
}

Monitor& JobSystem::get_monitor()
{
    return internal_->monitor;
}

void JobSystem::abort()
{
    // Join all workers as fast as possible
    try
    {
        shared_state_->running.store(false, std::memory_order_release);
        shared_state_->cv_wake.notify_all();
        for (size_t idx = 0; idx < threads_count_; ++idx)
        {
            workers_[idx].join();
        }
    }
    catch (const std::exception&)
    {
    }

    // We just killed all threads, including the logger thread
    // So we must go back to sync mode
    kb::log::Channel::set_async(nullptr);
    klog(log_channel_).uid("JobSystem").warn("PANIC: Essential work transfered to caller thread.");

    // Execute essential work on the caller thread
    for (size_t idx = 0; idx < threads_count_; ++idx)
    {
        workers_[idx].panic();
    }

    klog(log_channel_).uid("JobSystem").info("Shutting down.");

    exit(0);
}

void depth_first_walk(Job* job, std::function<bool(Job*)> visit)
{
    std::stack<Job*> stack;
    stack.push(job);
    while (!stack.empty())
    {
        job = stack.top();
        stack.pop();

        if (visit(job))
        {
            for (Job* child : *job)
            {
                stack.push(child);
            }
        }
    }
}

void Task::schedule(barrier_t barrier_id)
{
    JS_PROFILE_FUNCTION(js_->instrumentor_, js_->this_thread_id());

    // * Sanity check
    K_ASSERT(job_->in_count() == 0, "Tried to schedule a child task.", js_->log_channel_);

    size_t num_jobs = 1;

    if (job_->out_count() == 0)
    {
        // * Single job
        job_->barrier_id = barrier_id;
    }
    else
    {
        // * Walk job graph
        // Job graphs are DAGs, so we can safely use depth-first search
        std::unordered_set<Job*> marked;
        depth_first_walk(job_, [barrier_id, &marked](Job* job) {
            if (!marked.contains(job))
            {
                // Setup barriers on all child jobs
                job->barrier_id = barrier_id;
                marked.insert(job);
                // Explore children
                return true;
            }
            return false;
        });
        num_jobs = marked.size();
    }

    // * Setup dependency count
    if (barrier_id != k_no_barrier)
    {
        js_->get_barrier(job_->barrier_id).add_dependencies(num_jobs);
    }

    // * Schedule parent
    js_->try_schedule(job_, num_jobs);
}

bool Task::try_preempt_and_execute()
{
    /*
        NOTE(ndx): Job could very well be dead and returned to the pool, and
        I have now fucking way of knowing that. Task can be copied and moved,
        so I can't change an atomic flag from within the kernel.
        And I just refuse to pass the raw address of an atomic bool held by
        a shared_ptr, because just thinking about it makes me throw up in my
        mouth.
    */

    K_ASSERT(job_->in_count() == 0 && job_->out_count() == 0, "Tried to preempt a non-singular job.",
             js_->log_channel_);

    JobState expected1 = JobState::Idle;
    JobState expected2 = JobState::Pending;

    if (job_->exchange_state(expected1, JobState::Preempted) || job_->exchange_state(expected2, JobState::Preempted))
    {
        job_->kernel();

        if (job_->barrier_id != k_no_barrier)
        {
            js_->get_barrier(job_->barrier_id).remove_dependency();
        }

        job_->force_state(JobState::Processed);
        js_->release_job(job_);
        js_->shared_state_->pending.fetch_sub(1, std::memory_order_release);

        return true;
    }

    return false;
}

/// Get job metadata.
const JobMetadata& Task::meta() const
{
    return job_->meta;
}

void Task::wait(std::function<bool()> condition)
{
    JS_PROFILE_SCOPE(js_->instrumentor_, "Task::wait", js_->this_thread_id());
    js_->wait_until([this, &condition]() { return !is_processed() && condition(); });
}

bool Task::is_processed() const
{
    return job_->check_state(JobState::Processed);
}

void Task::add_child(const Task& task)
{
    job_->connect(*task.job_, task.job_);
}

void Task::add_parent(const Task& task)
{
    task.job_->connect(*job_, job_);
}

} // namespace th
} // namespace kb