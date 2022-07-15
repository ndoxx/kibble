#pragma once

#include "assert/assert.h"
#include "thread/job/config.h"
#include "thread/job/job_graph.h"
#include <future>
#include <map>
#include <vector>

namespace kb
{
class InstrumentationSession;

namespace memory
{
class HeapArea;
} // namespace memory

namespace th
{

using JobKernel = std::function<void(void)>;
using worker_affinity_t = uint32_t;
using tid_t = uint32_t;

/// A job with this affinity can be executed on any worker
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ANY = std::numeric_limits<worker_affinity_t>::max();
/// A job with this affinity should be executed on the main thread
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_MAIN = 1;
/// A job with this affinity should be executed on any background thread
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ASYNC = WORKER_AFFINITY_ANY & ~WORKER_AFFINITY_MAIN;

/**
 * @brief Metadata associated to a job.
 *
 */
struct JobMetadata
{
    JobMetadata() = default;
    JobMetadata(worker_affinity_t affinity, const std::string &profile_name);

    /// Workers this job can be pushed to
    worker_affinity_t worker_affinity = WORKER_AFFINITY_ANY;

#ifdef K_PROFILE_JOB_SYSTEM
    /// Descriptive name for the job (only used when profiling)
    std::string name;
#endif
};

/**
 * @brief Represents some amount of work to execute.
 *
 */
struct Job
{
    using JobNode = ProcessNode<Job *, k_max_parent_jobs, k_max_child_jobs>;

    /// Job metadata
    JobMetadata meta;
    /// The function to execute
    JobKernel kernel = JobKernel{};
    /// If true, job will not be returned to the pool once finished
    bool keep_alive = false;
    /// Dependency information and shared state
    JobNode node;

    /**
     * @internal
     * @brief Reset shared state
     *
     */
    inline void reset()
    {
        node.reset_state();
    }

    /**
     * @internal
     * @brief Add a job that can only be executed once this job was processed.
     *
     * @param job the child to add.
     */
    inline void add_child(Job *job)
    {
        node.connect(job->node, job);
    }

    /**
     * @internal
     * @brief Make this job dependent on another job.
     *
     * @param job the parent to add.
     */
    inline void add_parent(Job *job)
    {
        job->node.connect(node, this);
    }

    /**
     * @internal
     * @brief Non-blockingly check if this job's dependencies are satisfied.
     *
     * @return true If the dependencies are satisfied and the job can be scheduled.
     * @return false Otherwise.
     */
    inline bool is_ready() const
    {
        return node.is_ready();
    }

    /**
     * @internal
     * @brief Non-blockingly check if this job has been processed.
     *
     * @return true If the job has been processed.
     * @return false Otherwise.
     */
    inline bool is_processed() const
    {
        return node.is_processed();
    }

    /**
     * @internal
     * @brief Get the number of pending dependencies.
     *
     * @return size_t The dependency count.
     */
    inline size_t get_pending() const
    {
        return node.get_pending();
    }

    /**
     * @internal
     * @brief Mark this job processed, and update dependency information.
     *
     */
    inline void mark_processed()
    {
        node.mark_processed();
    }

    /**
     * @internal
     * @brief Try to mark this node scheduled.
     *
     * @return true If calling thread can safely schedule this job.
     * @return false Otherwise.
     */
    inline bool mark_scheduled()
    {
        return node.mark_scheduled();
    }
};

/**
 * @brief Job system configuration structure.
 *
 */
struct JobSystemScheme
{
    /// Maximum number of worker threads, if 0 => CPU_cores - 1
    size_t max_workers = 0;
    /// Maximum number of stealing attempts before moving to the next worker
    size_t max_stealing_attempts = 16;
};

class JobSystem;

/**
 * @brief Object returned by JobSystem when a job is created.
 * This class holds the job pointer as well as a promise and a future.
 * Any exception thrown by the kernel will be captured and rethrown
 * when the get() function is called.
 * This template is specialized for T = void. The void specialization has
 * no notion of promise and future, and handles exceptions differently.
 *
 * @note I use the following terminology: a "task" is essentially some
 * code to be run ("job") associated with some data to be produced ("future data").
 * So the user code sees "tasks", and the workers see "jobs".
 *
 * @tparam T
 */
template <typename T = void>
class Task
{
public:
    template <typename U>
    friend class Task;
    friend class JobSystem;

    // User code needs to access the promise somehow
    using TaskKernel = std::function<void(std::promise<T> &)>;

    Task() = default;

    /**
     * @brief Schedule job execution.
     * The number of pending jobs will be increased, the job dispatched and all worker threads will be awakened.
     *
     * @note Trying to schedule a child task will have no effect. A child job is always scheduled by
     * the worker that processed its parent, right after it has done so.
     *
     */
    inline void schedule();

    /// Get job metadata.
    inline const auto &meta() const
    {
        return job_->meta;
    }

    /**
     * @brief Retrieve data generated by the job.
     * This will create a sync point. It can be called without having to
     * call JobSystem::wait() if and only if you are certain that the job
     * is NOT executed on the main thread (affinity = WORKER_AFFINITY_ASYNC),
     * or it will deadlock.
     *
     * @return auto
     */
    inline auto get();

    /**
     * @brief Hold execution on the main thread until this job has been processed or the predicate returns false.
     *
     * @param condition While this predicate evaluates to true, the function waits for job completion.
     * When it evaluates to false, the function exits regardless of job completion. This can be used to implement
     * a timeout functionality.
     */
    inline void wait(std::function<bool()> condition = []() { return true; });

    /**
     * @brief Non-blockingly check if this job is processed.
     *
     * @return true it the job was processed, false otherwise
     */
    inline bool is_processed()
    {
        return job_->is_processed();
    }

    /**
     * @brief Get a copy of the shared future data.
     * This is used to pass data from parent job to child job.
     *
     * @return auto
     */
    inline auto get_future() const
    {
        return future_;
    }

    /**
     * @brief Add a job that can only be executed once this job was processed.
     *
     * @param task the child to add.
     * @tparam U future data type of the child job.
     */
    template <typename U>
    inline void add_child(const Task<U> &task)
    {
        job_->add_child(task.job_);
    }

    /**
     * @brief Make this task dependent on another one.
     *
     * @param task the parent to add.
     * @tparam U future data type of the parent job.
     */
    template <typename U>
    inline void add_parent(const Task<U> &task)
    {
        job_->add_parent(task.job_);
    }

private:
    Task(JobSystem *js, TaskKernel &&kernel, const JobMetadata &meta = JobMetadata{});

private:
    JobSystem *js_ = nullptr;
    Job *job_ = nullptr;
    std::shared_ptr<std::promise<T>> promise_ = nullptr;
    std::shared_future<T> future_; // Needs to be shared, as it can be accessed from another thread.
};

/**
 * @brief Specialized task returned by the JobSystem when no future data
 * type is explicitly specified on a call to JobSystem::create_job().
 * It has no notion of future data, so it holds no promise and no future,
 * the job kernel is argument-free, there is no get() method,
 * and captured exceptions are rethrown on a call to release().
 *
 * @tparam
 */
template <>
class Task<void>
{
public:
    friend class JobSystem;

    Task() = default;

    /// Schedule execution of this task.
    inline void schedule();

    /// Get job metadata.
    inline const auto &meta() const
    {
        return job_->meta;
    }

    /// Wait for task to be processed or the condition to evaluate to false.
    inline void wait(std::function<bool()> condition = []() { return true; });

    /// Non-blockingly check for task completion.
    inline bool is_processed()
    {
        return job_->is_processed();
    }

    /// Add a child task.
    inline void add_child(const Task &task)
    {
        job_->add_child(task.job_);
    }

    /// Add a parent task.
    inline void add_parent(const Task &task)
    {
        job_->add_parent(task.job_);
    }

private:
    Task(JobSystem *js, JobKernel &&kernel, const JobMetadata &meta = JobMetadata{});

private:
    JobSystem *js_ = nullptr;
    Job *job_ = nullptr;
};

struct SharedState;
class Scheduler;
class Monitor;
class WorkerThread;

/**
 * @brief Assign work to multiple worker threads.
 * The job system implementation is split into multiple single-responsibility components. All these components are
 * coordinated by this master system.
 *
 * @see Monitor Scheduler WorkerThread
 *
 */
class JobSystem
{
public:
    template <typename T>
    friend class Task;
    friend class WorkerThread;
    friend class DaemonScheduler;

    /**
     * @brief Get the total amount of memory needed for the job pool.
     *
     * @return size_t Minimal size in bytes to allocate in a memory::HeapArea.
     */
    static size_t get_memory_requirements();

    /**
     * @brief Construct a new Job System.
     * The job system uses a memory pool for fast job allocation, that's why a heap area is needed.
     * On creation, all subsystems are initialized. Then a certain number of worker threads are spawned, no more than
     * the maximum number prescribed by the scheme structure, and no more than the amount of CPU cores minus one, on the
     * present architecture.
     *
     * @param area heap area for the job pool
     * @param scheme configuration structure
     */
    JobSystem(memory::HeapArea &area, const JobSystemScheme &scheme);

    /**
     * @brief Calls shutdown() before destruction.
     *
     */
    ~JobSystem();

    /**
     * @brief Wait for all jobs to finish, join worker threads and destroy system storage
     *
     */
    void shutdown();

    /**
     * @brief Set the instrumentation session for profiling.
     *
     * @param session
     */
    inline void set_instrumentation_session(InstrumentationSession *session)
    {
        instrumentor_ = session;
    }

    /**
     * @brief Create a job with future data.
     *
     * @tparam T Future data type
     * @param kernel function containing code to execute.
     * @param meta job metadata. Can be used to give a unique label to this task and setup worker affinity.
     * @return A new task.
     */
    template <typename T>
    inline Task<T> create_task(typename Task<T>::TaskKernel &&kernel, const JobMetadata &meta = JobMetadata{})
    {
        return Task<T>(this, std::move(kernel), meta);
    }

    /**
     * @brief Create a job without specifying a future data type.
     *
     * @param kernel function containing code to execute.
     * @param meta job metadata. Can be used to give a unique label to this task and setup worker affinity.
     * @return A new task.
     */
    inline Task<void> create_task(JobKernel &&kernel, const JobMetadata &meta = JobMetadata{})
    {
        return Task<void>(this, std::move(kernel), meta);
    }

    /**
     * @brief Non-blockingly check if any worker threads are busy.
     *
     * @return true is at least one worker thread is currently unidle
     * @return false otherwise
     */
    bool is_busy() const;

    /**
     * @brief Wait for an input condition to become false, synchronous work may be executed in the meantime.
     * @note Will automatically release all finished jobs.
     *
     * @param condition predicate that will keep the function busy till it evaluates to false
     */
    void wait_until(std::function<bool()> condition);

    /**
     * @brief Hold execution on this thread until all jobs are processed or the input predicate returns false.
     * Allows to wait for all jobs to be processed. This marks a sync point in the caller code.
     * @note Will automatically release all finished jobs.
     *
     * @param condition While this predicate evaluates to true, the function waits for job completion.
     * When it evaluates to false, the function exits regardless of pending jobs. This can be used to implement
     * a timeout functionality.
     */
    inline void wait(std::function<bool()> condition = []() { return true; })
    {
        wait_until([this, &condition]() { return is_busy() && condition(); });
    }

    /// Get the list of workers (non-const).
    inline auto &get_workers()
    {
        return workers_;
    }

    /// Get the list of workers (const).
    inline const auto &get_workers() const
    {
        return workers_;
    }

    /// Get the number of threads
    inline size_t get_threads_count() const
    {
        return threads_count_;
    }

    /// Get the number of CPU cores on this machine.
    inline size_t get_CPU_cores_count() const
    {
        return CPU_cores_count_;
    }

    /// Get the configuration object passed to this system at construction.
    inline const auto &get_scheme() const
    {
        return scheme_;
    }

    /// Get the worker at input index (non-const).
    inline auto &get_worker(size_t idx)
    {
        return *workers_[idx];
    }

    /// Get the worker at input index (const).
    inline const auto &get_worker(size_t idx) const
    {
        return *workers_[idx];
    }

    /// Get the monitor (non-const).
    inline auto &get_monitor()
    {
        return *monitor_;
    }

    /// Get the monitor (const).
    inline const auto &get_monitor() const
    {
        return *monitor_;
    }

    /// Get the shared state (non-const).
    inline auto &get_shared_state()
    {
        return *ss_;
    }

    /// Get the shared state (const).
    inline const auto &get_shared_state() const
    {
        return *ss_;
    }

    /// Get the tid of the current thread.
    inline tid_t this_thread_id() const
    {
        return thread_ids_.at(std::this_thread::get_id());
    }

    /// Get pointer to instrumentation session
    inline InstrumentationSession *get_instrumentation_session()
    {
        return instrumentor_;
    }

private:
    /**
     * @internal
     * @brief Create a new job.
     * @note The returned job belongs to an internal job pool and should never be deleted manually. Once caller code is
     * done with the job, the function release_job() should be called to return the job to the pool.
     *
     * @param kernel function containing code to execute.
     * @param meta job metadata. Can be used to give a unique label to this job and setup worker affinity.
     * @return a new job from the pool
     */
    Job *create_job(JobKernel &&kernel, const JobMetadata &meta = JobMetadata{});

    /**
     * @internal
     * @brief Return job to the pool.
     * Also writes job profiling data to the instrumentation session if profiling is
     * enabled.
     *
     * @note Can be called concurrently.
     *
     * @param job the job to release
     */
    void release_job(Job *job);

    /**
     * @internal
     * @brief Schedule job execution.
     * The number of pending jobs will be increased, the job dispatched and all worker threads will be awakened.
     *
     * @note Only orphan jobs can be scheduled. A job is orphan if its parent has been processed, or if it
     * had no parent to begin with. Scheduling a non-orphan job will trigger an assertion.
     *
     * @param job the job to submit
     */
    void schedule(Job *job);

    /**
     * @internal
     * @brief Hold execution on the main thread until a given job has been processed or the predicate returns false.
     *
     * @param job the job to wait for
     * @param condition While this predicate evaluates to true, the function waits for job completion.
     * When it evaluates to false, the function exits regardless of job completion. This can be used to implement
     * a timeout functionality.
     */
    inline void wait_for(
        Job *job, std::function<bool()> condition = []() { return true; })
    {
        wait_until([this, &condition, job]() { return !is_work_done(job) && condition(); });
    }

    /**
     * @internal
     * @brief Non-blockingly check if a job is processed.
     *
     * @param job the job
     * @return true it the job was processed, false otherwise
     */
    bool is_work_done(Job *job) const;

private:
    size_t CPU_cores_count_ = 0;
    size_t threads_count_ = 0;
    JobSystemScheme scheme_;
    std::vector<std::unique_ptr<WorkerThread>> workers_;
    std::unique_ptr<Scheduler> scheduler_;
    std::unique_ptr<Monitor> monitor_;
    std::shared_ptr<SharedState> ss_;
    std::map<std::thread::id, tid_t> thread_ids_;
    InstrumentationSession *instrumentor_ = nullptr;
};

template <typename T>
Task<T>::Task(JobSystem *js, Task<T>::TaskKernel &&kernel, const JobMetadata &meta)
    : js_(js), promise_(new std::promise<T>)
{
    future_ = promise_->get_future();
    /*
        Job kernel is a void wrapper around the templated kernel passed in this call. This allows
        some form of type erasure: user code can access promise as a kernel argument, but the
        workers only see a void function call.
        The promise is stored as a shared_ptr and captured by value, as a plain std::promise cannot
        be captured by the lambda (non-copyable). The kernel can be moved.
    */
    job_ = js_->create_job(
        [promise = promise_, kernel = std::move(kernel)]() {
#ifdef K_ENABLE_JOB_EXCEPTIONS
            try
            {
                kernel(*promise);
            }
            catch (...)
            {
                // Store any exception thrown by the kernel function,
                // it will be rethrown when the future's get() function is called.
                promise->set_exception(std::current_exception());
            }
#else
            kernel(*promise);
#endif
        },
        meta);
}

template <typename T>
inline void Task<T>::schedule()
{
    js_->schedule(job_);
}

template <typename T>
inline auto Task<T>::get()
{
    // We want to avoid the situation where the caller thread calls a blocking get() on one of its own pending tasks.
    [[maybe_unused]] bool maybe_handled_by_caller = bool(job_->meta.worker_affinity & (1 << js_->this_thread_id()));
    [[maybe_unused]] bool processed = js_->is_work_done(job_);
    K_ASSERT(processed || !maybe_handled_by_caller, "Possible self deadlock.");

    return future_.get();
}

template <typename T>
inline void Task<T>::wait(std::function<bool()> condition)
{
    js_->wait_for(job_, condition);
}

inline void Task<void>::schedule()
{
    js_->schedule(job_);
}

inline void Task<void>::wait(std::function<bool()> condition)
{
    js_->wait_for(job_, condition);
}

} // namespace th
} // namespace kb