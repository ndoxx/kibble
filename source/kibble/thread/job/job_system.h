#pragma once

#include "../../util/internal.h"
#include "barrier_id.h"
#include "job_meta.h"

#include <future>
#include <unordered_map>

namespace kb::log
{
class Channel;
}

namespace kb
{
class InstrumentationSession;

namespace memory
{
class HeapArea;
} // namespace memory

namespace th
{

struct SharedState;
class Monitor;
class WorkerThread;
class Task;
class Barrier;
struct Job;

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
    friend class Task;
    friend class WorkerThread;
    friend class Scheduler;
    friend class DaemonScheduler;

    /**
     * @brief Job system configuration structure.
     *
     */
    struct Config
    {
        /// Maximum number of worker threads, if 0 => CPU_cores - 1
        size_t max_workers = 0;
        /// Maximum number of stealing attempts before moving to the next worker
        size_t max_stealing_attempts = 16;
        /// Maximum number of barriers
        size_t max_barriers = 16;
    };

    /**
     * @brief Get the total amount of memory needed for the job pool.
     *
     * @return size_t Minimal size in bytes to allocate in a memory::HeapArea.
     */
    static size_t get_memory_requirements(const Config& scheme);

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
    JobSystem(memory::HeapArea& area, const Config& scheme, const kb::log::Channel* log_channel = nullptr);

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
    inline void set_instrumentation_session(InstrumentationSession* session)
    {
        instrumentor_ = session;
    }

    /**
     * @brief Create a barrier to wait on multiple jobs
     *
     * @note The barrier is already allocated by the system during construction,
     * this function simply marks it as used.
     *
     * @return barrier_t Barrier ID
     */
    barrier_t create_barrier();

    /**
     * @brief Destroy a barrier
     *
     * @note The barrier will be physically deallocated on system destruction,
     * this function simply marks it as unused.
     *
     * @param id Barrier ID
     */
    void destroy_barrier(barrier_t id);

    /**
     * @brief Create a task
     *
     * @tparam FuncT type of the function to execute
     * @tparam ArgsT type of the arguments to be passed to the function
     * @param meta job metadata. Can be used to give a unique label to this task and setup worker affinity.
     * @param function function containing code to execute.
     * @param args arguments to be passed to the function
     * @return a pair containing the created task and its future result (std::shared_future)
     */
    template <typename FuncT, typename... ArgsT>
    inline auto create_task(JobMetadata&& meta, FuncT&& function, ArgsT&&... args)
    {
        auto promise = std::make_shared<std::promise<std::invoke_result_t<FuncT, ArgsT...>>>();
        std::shared_future<std::invoke_result_t<FuncT, ArgsT...>> future = promise->get_future();
        return std::make_pair(Task(this, std::forward<JobMetadata>(meta), std::forward<FuncT>(function),
                                   std::move(promise), std::forward<ArgsT>(args)...),
                              std::move(future));
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
    void wait(std::function<bool()> condition = []() { return true; });

    /**
     * @brief Hold execution on this thread until all jobs under specified barrier (and its dependents) are processed.
     *
     * @param barrier_id
     */
    void wait_on_barrier(uint64_t barrier_id);

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

    inline const Config& get_config() const
    {
        return config_;
    }

    /// Get the tid of the current thread.
    inline tid_t this_thread_id() const
    {
        return thread_ids_.at(std::this_thread::get_id());
    }

    /// Get pointer to instrumentation session
    inline InstrumentationSession* get_instrumentation_session()
    {
        return instrumentor_;
    }

    /**
     * @brief Force workers to join, and execute essential work before shutdown
     *
     * @warning This is highly experimental
     *
     */
    void abort();

private:
    /// Get the worker at input index (non-const).
    WorkerThread& get_worker(size_t idx);

    /// Get the worker at input index (const).
    const WorkerThread& get_worker(size_t idx) const;

    /// Get the monitor (non-const).
    Monitor& get_monitor();

    /**
     * @brief Get barrier object by ID
     *
     * @param id Barrier ID
     */
    Barrier& get_barrier(barrier_t id);

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
    Job* create_job(std::function<void(void)>&& kernel, JobMetadata&& meta = JobMetadata{});

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
    void release_job(Job* job);

    /**
     * @internal
     * @brief Try to schedule job execution.
     * Atomically exchange the job state to Pending. If exchange fails, return false.
     * The number of pending jobs will be increased, the job dispatched and all worker threads will be awakened.
     *
     * @note Only orphan jobs can be scheduled. A job is orphan if its parent has been processed, or if it
     * had no parent to begin with. Scheduling a non-orphan job will trigger an assertion.
     * @note When scheduling a (topmost) parent job, pre-calculate the total number of jobs to schedule,
     * like it's done in the Task::schedule() function with a depth-first traversal of the job graph.
     * @note When scheduling a child job, set num_jobs to 0.
     *
     * @param job the job to submit
     * @param num_jobs total number of jobs to schedule (this job and its children) when top parent, 0 when child job
     * @return true if job was successfully scheduled, false otherwise
     */
    bool try_schedule(Job* job, size_t num_jobs);

private:
    Config config_;

    struct Internal;
    friend struct internal_deleter::Deleter<Internal>;
    internal_ptr<Internal> internal_{nullptr};

    size_t CPU_cores_count_{0};
    size_t threads_count_{0};
    std::unordered_map<std::thread::id, tid_t> thread_ids_;
    Barrier* barriers_{nullptr};
    SharedState* shared_state_{nullptr};
    WorkerThread* workers_{nullptr};
    InstrumentationSession* instrumentor_{nullptr};
    const kb::log::Channel* log_channel_{nullptr};
};

/**
 * @brief Object returned by JobSystem when a job is created.
 *
 * This class has been redesigned following ChiliTomatoNoodle's philosophy,
 * see: - https://www.youtube.com/watch?v=sTR16hSzsiI
 *      - https://github.com/planetchili/mt-next/blob/master/main.cpp
 *
 * @note I use the following terminology: a "task" is essentially some
 * code to be run ("job") associated with some data to be produced ("future data").
 * So the user code sees "tasks", and the workers see "jobs".
 *
 * @tparam T
 */
class Task
{
public:
    friend class JobSystem;

    // Default constructible, movable, non-copyable
    Task() = default;

    /**
     * @brief Schedule job execution.
     * The number of pending jobs will be increased, the job dispatched and all worker threads will be awakened.
     *
     * @note Trying to schedule a child task will assert, only schedule topmost parent tasks.
     *
     */
    void schedule(barrier_t barrier_id = k_no_barrier);

    /**
     * @brief Try to execute the job on this thread.
     * Only singular jobs can be preempted at the moment.
     * If the job is already in a worker queue, it will be safely skipped.
     *
     * @warning Experimental and unsafe. Task object cannot know if the job is dead
     * and in the pool, some intense necrophilic action could be going on after a call.
     * Memory is intact due to arena allocation, and the job pointer should be far away
     * in the free list, but it's morally wrong.
     * The only reason I considered doing this in the first place was because my Jolt
     * job system adapter needs some kind of preemption mechanism.
     *
     * @return true if the job was prempted
     * @return false if the job was already executing on a worker thread, or finished.
     */
    bool try_preempt_and_execute();

    /// Get job metadata.
    const JobMetadata& meta() const;

    /**
     * @brief Hold execution on the main thread until this job has been processed or the predicate returns false.
     *
     * @param condition While this predicate evaluates to true, the function waits for job completion.
     * When it evaluates to false, the function exits regardless of job completion. This can be used to implement
     * a timeout functionality.
     */
    void wait(std::function<bool()> condition = []() { return true; });

    /**
     * @brief Non-blockingly check if this job is processed.
     *
     * @return true it the job was processed, false otherwise
     */
    bool is_processed() const;

    /**
     * @brief Add a job that can only be executed once this job was processed.
     *
     * @note The child job will automatically inherit the barrier ID of this job.
     *
     * @param task the child to add.
     */
    void add_child(const Task& task);

    /**
     * @brief Make this task dependent on another one.
     *
     * @note This job will automatically inherit the barrier ID of the parent.
     *
     * @param task the parent to add.
     */
    void add_parent(const Task& task);

private:
    /**
     * @internal
     * @brief Construct a new Task
     *
     * @tparam FuncT type of function to execute
     * @tparam PromiseT promise type
     * @tparam ArgsT kernel arguments pack
     * @param js job system instance
     * @param meta job metadata
     * @param func function to execute
     * @param promise promise to store the result
     * @param args kernel arguments
     */
    template <typename FuncT, typename PromiseT, typename... ArgsT>
    Task(JobSystem* js, JobMetadata&& meta, FuncT&& func, PromiseT&& promise, ArgsT&&... args) : js_(js)
    {
        /*
            Job kernel is a void wrapper around the templated kernel passed in this call. This allows
            some form of type erasure: user code can access kernel arguments, but the
            workers only see a void function call.
            The promise is stored as a shared_ptr, as a plain std::promise cannot
            be captured by the lambda (non-copyable). The kernel and its arguments can be moved.
        */
        auto kernel = [func = std::forward<FuncT>(func), promise = std::forward<PromiseT>(promise),
                       ... args = std::forward<ArgsT>(args)]() mutable {
            try
            {
                // void functions need special care
                if constexpr (std::is_void_v<std::invoke_result_t<FuncT, ArgsT...>>)
                {
                    func(std::forward<ArgsT>(args)...);
                    promise->set_value();
                }
                else
                {
                    // set the promise value as the kernel return value
                    promise->set_value(func(std::forward<ArgsT>(args)...));
                }
            }
            catch (...)
            {
                // Store any exception thrown by the kernel function,
                // it will be rethrown when the future's get() function is called.
                promise->set_exception(std::current_exception());
            }
        };

        // Let the JobSystem perform job allocation, move the kernel
        job_ = js_->create_job(std::move(kernel), std::move(meta));
    }

private:
    JobSystem* js_{nullptr};
    Job* job_{nullptr};
};

} // namespace th
} // namespace kb