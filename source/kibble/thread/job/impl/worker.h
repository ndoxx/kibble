#pragma once
#include "thread/alignment.h"
#include "thread/job/impl/common.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

namespace kb
{
namespace th
{

/**
 * @brief Properties of a worker thread.
 *
 */
struct WorkerProperties
{
    /// false if main thread, true otherwise
    bool is_background = false;
    /// maximum allowable attempts at stealing a job
    size_t max_stealing_attempts = 16;
    /// worker id
    tid_t tid;
};

struct Job;

/**
 * @brief Data common to all worker threads.
 * All members are page aligned, so as to avoid false sharing.
 *
 */
struct SharedState
{
    /// Number of tasks left
    PAGE_ALIGN std::atomic<uint64_t> pending = {0};
    /// Flag to signal workers when they should stop and join
    PAGE_ALIGN std::atomic<bool> running = {true};
    /// Memory arena to store job structures (page aligned)
    PAGE_ALIGN JobPoolArena job_pool;
    /// To wake worker threads
    PAGE_ALIGN std::condition_variable cv_wake;
    /// Workers wait on this one when they're idle
    PAGE_ALIGN std::mutex wake_mutex;
};

class JobSystem;

/**
 * @brief Represents a worker thread.
 * A naive worker thread implementation would essentially continuously execute jobs from its job queue until the queue
 * is empty, at which point it would become idle. This however can prove inefficient when the load is not evenly
 * balanced among workers: some workers would basically do nothing while others would have piles of work to process.
 * This implementation of a worker thread allows for work stealing, in an attempt to enhance load balancing naturally.
 * When a worker has processed all jobs in its queue, it will try to pop jobs from other worker's queues selected at
 * random.
 *
 * The job queue used behind the scene is a lock-free atomic queue, so there is no contention due to dispatching or work
 * stealing. This makes this implementation thread-safe and quite fast.
 *
 */
class WorkerThread
{
public:
    /**
     * @brief All possibles states a worker can be in.
     * The state of a worker is stored in an atomic member so as to avoid sync points when querying other workers'
     * state.
     *
     */
    enum class State : uint8_t
    {
        /// The worker does nothing
        Idle,
        /// The worker is executing jobs
        Running,
        // The worker is stopping, and the thread will be joinable
        Stopping
    };

    /**
     * @brief Construct and configure a new Worker Thread.
     *
     * @param props properties this worker should observe
     * @param js job system instance
     */
    WorkerThread(const WorkerProperties &props, JobSystem &js);

    /**
     * @brief Spawn a thread for this worker.
     *
     */
    void spawn();

    /**
     * @brief Join this worker's thread.
     *
     */
    void join();

    /**
     * @brief The Scheduler calls this function to enqueue a job.
     *
     * @param job the job to push
     */
    void submit(Job *job);

    /**
     * @brief Only the main thread calls this function to pop and execute a single job.
     * This function can potentially steal jobs from background workers. It is used to assist background threads while
     * waiting on the main thread.
     *
     * @return true if the pop operation succeeded
     * @return false otherwise
     */
    bool foreground_work();

    /// Check whether this worker is a background worker.
    inline bool is_background() const
    {
        return props_.is_background;
    }

    /**
     * @brief Get this worker's ID.
     *
     * @return tid_t
     */
    inline tid_t get_tid() const
    {
        return props_.tid;
    }

    /**
     * @brief Get the system thread id.
     * In association with the JobSystem's thread id map, this allows to know the tid_t thread index
     * of the current context.
     *
     * @return std::thread::id
     */
    inline std::thread::id get_native_thread_id() const
    {
        return thread_.get_id();
    }

    /**
     * @brief Atomically get this worker's state.
     *
     * @return State
     */
    inline State query_state() const
    {
        return state_.load(std::memory_order_acquire);
    }

#if K_PROFILE_JOB_SYSTEM
    /**
     * @internal
     * @brief Get this worker's activity report
     *
     * @return const auto&
     */
    inline const auto &get_activity() const
    {
        return activity_;
    }

    /**
     * @internal
     * @brief Get this worker's activity report
     *
     * @return auto&
     */
    inline auto &get_activity()
    {
        return activity_;
    }
#endif

private:
    /**
     * @internal
     * @brief Thread loop.
     *
     */
    void run();

    /**
     * @internal
     * @brief Get next job in the queue or steal work from another worker.
     * First, the worker tries to pop a job from the queue. If the queue is empty, it will try to pop a job from a
     * randomly selected worker's queue. If the job has incompatible affinity the job will be resubmitted.
     *
     * @param job Output variable that will contain the next job, or will be left uninitialized if no job could be
     * obtained.
     * @return true if a job was obtained
     * @return false otherwise
     */
    bool get_job(Job *&job);

    /**
     * @internal
     * @brief Execute a job.
     *
     * @param job the job to execute
     */
    void process(Job *job);

    /**
     * @internal
     * @brief If a job has children, schedule them to a random compatible queue.
     *
     * Called by process, after a job kernel has been executed.
     *
     * @param job
     */
    void schedule_children(Job *job);

    /**
     * @internal
     * @brief Return the next round robin index in stealable workers list.
     *
     * @return size_t
     */
    inline size_t rr_next()
    {
        return (stealing_round_robin_++) % (stealable_workers_.size());
    }

private:
    WorkerProperties props_;
    JobSystem &js_;
    SharedState &ss_;
    std::atomic<State> state_;
    std::thread thread_;

#if K_PROFILE_JOB_SYSTEM
    WorkerActivity activity_;
#endif
    std::vector<WorkerThread *> stealable_workers_;
    size_t stealing_round_robin_ = 0;

    PAGE_ALIGN JobQueue<Job *> jobs_; // MPMC queue
};

} // namespace th
} // namespace kb