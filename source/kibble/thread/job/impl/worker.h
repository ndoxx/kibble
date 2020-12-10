#pragma once
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

struct WorkerProperties
{
    bool is_background = false;
    bool can_steal = false;
    size_t max_stealing_attempts = 16;
    tid_t tid;
};

struct Job;
// Data common to all worker threads
struct SharedState
{
    PAGE_ALIGN std::atomic<uint64_t> pending = {0}; // Number of tasks left
    PAGE_ALIGN std::atomic<bool> running = {true};  // Flag to signal workers when they should stop and join
    PAGE_ALIGN PoolArena job_pool;                  // Memory arena to store job structures (page aligned)
    PAGE_ALIGN std::condition_variable cv_wake;     // To wake worker threads
    PAGE_ALIGN std::mutex wake_mutex;               // Workers wait on this one when they're idle
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

    WorkerThread(const WorkerProperties&, JobSystem&);

    // Spawn a thread for this worker
    void spawn();
    // Join this worker's thread
    void join();
    // Scheduler calls this function to enqueue a job
    void submit(Job* job);
    // Main thread ONLY calls this function to pop and execute a single job
    // Returns true if the pop operation succeeded, false otherwise
    bool foreground_work();

    inline tid_t get_tid() const { return props_.tid; }
    inline State query_state() const { return state_.load(std::memory_order_relaxed); }
#if PROFILING
    inline const auto& get_activity() const { return activity_; }
    inline auto& get_activity() { return activity_; }
#endif

private:
    // Thread loop
    void run();
    // Get next job in the queue or steal work from another worker
    bool get_job(Job*& job);
    // Execute a job
    void process(Job* job);

private:
    WorkerProperties props_;
    JobSystem& js_;
    SharedState& ss_;
    std::atomic<State> state_;
    std::thread thread_;

#if PROFILING
    WorkerActivity activity_;
#endif

    JobQueue<Job*> jobs_;
};

} // namespace th
} // namespace kb