#pragma once
#include "thread/alignment.h"
#include "thread/job/impl/common.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

namespace kb::th
{

/**
 * @brief Properties of a worker thread.
 *
 */
struct WorkerProperties
{
    /// maximum allowable attempts at stealing a job
    size_t max_stealing_attempts = 16;
    /// worker id
    tid_t tid;
};

struct Job;
struct JobMetadata;

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
 * When a worker has processed all jobs in its queues, it will try to pop jobs from other worker's public queues.
 *
 * The job queues used behind the scene are lock-free atomic queues, so there is low contention due to dispatching or
 * work stealing. This makes this implementation thread-safe and quite fast.
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

    static constexpr size_t Q_PUBLIC = 0;
    static constexpr size_t Q_PRIVATE = 1;

    /**
     * @brief Construct and configure a new Worker Thread.
     *
     * @param props properties this worker should observe
     * @param js job system instance
     */
    WorkerThread(const WorkerProperties& props, JobSystem& js);

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
     * @brief The Scheduler calls this function to enqueue a job in one of the queues.
     *
     * @note At the moment, the only non-stealable jobs are the ones to be executed on the main thread,
     * so we could argue that the 'stealable' parameter is superfluous, as this information is contained in the
     * metadata. But this is subject to change when I develop work groups and recurrent tasks.
     *
     * @param job the job to push
     * @param stealable if the job is stealable it will be put in the public queue, otherwise, in the private queue.
     */
    void submit(Job* job, bool stealable);

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
        return props_.tid != 0;
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

    /**
     * @brief Check if there are pending jobs in the queues.
     *
     * @return true if either the private or public queue had pending jobs.
     * @return false otherwise.
     */
    inline bool had_pending_jobs() const
    {
        return !queues_[Q_PUBLIC].was_empty() || !queues_[Q_PRIVATE].was_empty();
    }

    /**
     * @internal
     * @brief Get this worker's activity report
     *
     * @return const auto&
     */
    inline const auto& get_activity() const
    {
        return activity_;
    }

    /**
     * @internal
     * @brief Get this worker's activity report
     *
     * @return auto&
     */
    inline auto& get_activity()
    {
        return activity_;
    }

    void panic();

private:
    /**
     * @internal
     * @brief Thread loop.
     *
     */
    void run();

    /**
     * @internal
     * @brief Get next locally available job or steal work from another worker.
     * First, the worker tries to pop a job from its private queue. If the queue is empty, it will try to pop a job
     * from the public queue. If the queue is empty, it will try to steal work from the next worker in the
     * round robin.
     *
     * @param job Output variable that will contain the next job, or will be left uninitialized if no job could be
     * obtained.
     * @return true if a job was obtained
     * @return false otherwise
     */
    bool get_job(Job*& job);

#ifdef K_ENABLE_WORK_STEALING
    /**
     * @internal
     * @brief Try to steal a job from the next worker in the round robin.
     *
     * @param job Output variable that will contain the next job, or will be left uninitialized if no job could be
     * obtained.
     * @return true if a job was obtained
     * @return false otherwise
     */
    bool steal_job(Job*& job);
#endif

    /**
     * @internal
     * @brief Execute a job.
     *
     * @param job the job to execute
     */
    void process(Job* job);

    /**
     * @internal
     * @brief If a job has children with satisfied dependencies, schedule them.
     *
     * Called by process(), after a job kernel has been executed.
     *
     * @param job
     */
    void schedule_children(Job* job);

    /**
     * @internal
     * @brief Return the next tid in the round robin.
     *
     * @return tid_t
     */
    inline tid_t rr_next()
    {
        return stealable_workers_[(stealing_round_robin_++) % (stealable_workers_.size())];
    }

private:
    WorkerProperties props_;
    JobSystem& js_;
    SharedState& ss_;
    std::atomic<State> state_;
    std::thread thread_;

    WorkerActivity activity_;
    std::vector<tid_t> stealable_workers_;
    size_t stealing_round_robin_ = 0;

    PAGE_ALIGN std::array<JobQueue<Job*>, 2> queues_;
};

} // namespace kb::th