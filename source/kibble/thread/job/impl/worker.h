#pragma once

#include "thread/job/impl/common.h"
#include "thread/sync.h"
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

namespace kb
{
namespace th
{
enum class SchedulerExecutionPolicy : uint8_t
{
    automatic, // Job may be executed synchronously during wait() or asynchronously
    deferred,  // Job execution is synchronous and deferred to the next wait() call
    async      // Job will be executed asynchronously
};

struct JobMetadata
{
    uint64_t label = 0;                        // Number associated to a particular kind of job for monitoring purposes
    int64_t execution_time_us = 0;             // The time it took to execute the job
    SchedulerExecutionPolicy execution_policy; // Where the job needs to be executed
};

struct Job
{
    JobKernel kernel = []() {}; // The function to execute
    JobHandle handle = 0;       // Handle associated to this job
    JobHandle parent = 0;       // Handle associated to this job
    JobMetadata metadata;       // Additional job info
};

// Data common to all worker threads
struct SharedState
{
    PAGE_ALIGN std::atomic<uint64_t> pending = {0}; // Number of tasks left
    PAGE_ALIGN std::atomic<bool> running = {true};  // Flag to signal workers when they should stop and join
    PAGE_ALIGN PoolArena job_pool;                  // Memory arena to store job structures (page aligned)
    PAGE_ALIGN HandlePool handle_pool;              // That's where they come from
    PAGE_ALIGN std::condition_variable cv_wake;     // To wake worker threads
    PAGE_ALIGN std::mutex wake_mutex;               // Workers wait on this one when they're idle
    PAGE_ALIGN SpinLock handle_lock;                // Fast lock to secure handle pool access
    PAGE_ALIGN DeadJobQueue<Job*> dead_jobs;        // Central queue for processed jobs
};

class JobSystem;
class WorkerThread
{
public:
    enum class State : uint8_t
    {
        Idle,
        Running,
        Stopping
    };

    WorkerThread(tid_t tid, bool background, bool can_steal, JobSystem& js);

    // Main thread ONLY calls this function to pop and execute a single job
    // Returns true if the pop operation succeeded, false otherwise
    bool foreground_work();

    // Spawn a thread for this worker
    inline void spawn()
    {
        if(background_)
            thread_ = std::thread(&WorkerThread::run, this);
    }
    // Join this worker's thread
    inline void join()
    {
        if(background_)
            thread_.join();
    }
    // Scheduler calls this function to enqueue a job
    inline void submit(Job* job)
    {
        ANNOTATE_HAPPENS_BEFORE(&jobs_); // Avoid false positives with TSan
        jobs_.push(job);
    }

    inline tid_t get_tid() const { return tid_; }
    inline State get_state() const { return state_.load(std::memory_order_relaxed); }
    inline size_t get_queue_size() const { return jobs_.was_size(); }

#if PROFILING
    inline const auto& get_activity() const { return activity_; }
    inline auto& get_activity() { return activity_; }
#endif

private:
    bool get_job(Job*& job);
    // Execute a job
    void process(Job* job);
    // Thread loop
    void run();
    // Select a (different) worker at random
    WorkerThread* random_worker();
    // Try to steal a job from another worker
    inline bool try_steal(Job*& job)
    {
        auto* w = random_worker();
        ANNOTATE_HAPPENS_AFTER(&w->jobs_); // Avoid false positives with TSan
        return w->jobs_.try_pop(job);
    }
    // Helper function to (thread-safely) release a job handle
    inline void release_handle(JobHandle handle)
    {
        const std::scoped_lock<SpinLock> lock(ss_.handle_lock);
        ss_.handle_pool.release(handle);
    }
    // Check if a job is still pending
    inline bool is_pending(JobHandle handle) const
    {
        const std::scoped_lock<SpinLock> lock(ss_.handle_lock);
        return ss_.handle_pool.is_valid(handle);
    }

private:
    tid_t tid_;
    bool background_;
    bool can_steal_;
    size_t push_pop_loop_detector_;
    std::atomic<State> state_;
    JobSystem& js_;
    SharedState& ss_;
    std::thread thread_;
    std::random_device rd_;
    std::uniform_int_distribution<size_t> dist_;

#if PROFILING
    WorkerActivity activity_;
#endif

    JobQueue<Job*> jobs_;
};

} // namespace th
} // namespace kb