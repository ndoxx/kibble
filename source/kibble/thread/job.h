#pragma once

#include <functional>
#include <memory>

namespace kb
{
namespace memory
{
class HeapArea;
}

using JobFunction = std::function<void(void)>;
using JobHandle = std::size_t;

enum class ExecutionPolicy : uint8_t
{
    automatic, // Job may be executed synchronously during wait() or asynchronously
    deferred,  // Job execution is synchronous and deferred to the next wait() call
    async,     // Job will be executed asynchronously
};

enum class SchedulingAlgorithm : uint8_t
{
    round_robin, // Round-robin selection of worker threads
    associative, // Associate job labels to execution times for smarter future decisions
};

struct JobSystemScheme
{
    size_t max_threads = 0;             // Maximum number of worker threads, if 0 => CPU_cores - 1
    bool enable_foreground_work = true; // Allow main thread to share some load with workers
    bool enable_work_stealing = true;   // Allow idle workers to steal jobs from their siblings
    SchedulingAlgorithm scheduling_algorithm = SchedulingAlgorithm::round_robin;
};

class Scheduler;
class WorkerThread;
struct SharedState;
class JobSystem
{
public:
    JobSystem(memory::HeapArea& area, const JobSystemScheme& = {});
    ~JobSystem();

    // Wait for all jobs to finish, join worker threads and destroy system storage
    void shutdown();
    // Enqueue a new job and return a handle
    JobHandle dispatch(JobFunction&& function, uint64_t label = 0, ExecutionPolicy policy = ExecutionPolicy::automatic);
    // Immediate asynchronous execution
    JobHandle async(JobFunction&& function, uint64_t label = 0);
    // Wait for input condition to become false, synchronous work may be executed in the meantime
    void wait_untill(std::function<bool()> condition);
    // Hold execution on this thread untill all jobs are processed or predicate returns false
    void wait(std::function<bool()> condition = []() { return true; });
    // Hold execution on this thread untill a given job has been processed or predicate returns false
    void wait_for(JobHandle handle, std::function<bool()> condition = []() { return true; });
    // Non-blockingly check if any worker threads are busy
    bool is_busy() const;
    // Non-blockingly check if a job is processed
    bool is_work_done(JobHandle handle) const;
    // Call this regularly, all scheduled tasks will be performed
    void update();

    inline const JobSystemScheme& get_scheme() const { return scheme_; }
    inline size_t get_threads_count() const { return threads_count_; }
    inline WorkerThread* get_worker(size_t idx) { return threads_[idx]; }

private:
    friend class WorkerThread;

    // Return dead jobs to their respective pools
    void cleanup();
    inline Scheduler* get_scheduler() { return scheduler_; }

private:
    size_t CPU_cores_count_ = 0;
    size_t threads_count_ = 0;
    JobSystemScheme scheme_;
    std::vector<WorkerThread*> threads_;
    Scheduler* scheduler_;
    std::shared_ptr<SharedState> ss_;
};

} // namespace kb