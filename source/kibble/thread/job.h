#pragma once

#include <functional>
#include <memory>

namespace kb
{
namespace memory
{
class HeapArea;
}

struct JobSystemScheme
{
    size_t max_threads = 0; // Maximum number of worker threads, if 0 => CPU_cores - 1
    bool enable_foreground_work = true; // Allow main thread to share some load with workers
    bool enable_work_stealing = true; // Allow idle workers to steal jobs from their siblings
};

class WorkerThread;
class JobSystem
{
public:
    using JobFunction = std::function<void(void)>;
    using JobHandle = size_t;

    JobSystem(memory::HeapArea& area, const JobSystemScheme& = {});
    ~JobSystem();

    // Wait for all jobs to finish, join worker threads and destroy system storage
    void shutdown();
    // Enqueue a new job for asynchronous execution and return a handle for this job
    JobHandle schedule(JobFunction function, bool force_async = false);
    // Hold execution on this thread till all jobs are processed
    void wait();
    // Non-blockingly check if any worker threads are busy
    bool is_busy();
    // Non-blockingly check if a job is processed
    bool is_work_done(JobHandle handle);
    // Call this regularly, all scheduled tasks will be performed
    void update();

private:
    friend class WorkerThread;

    // Return dead jobs to their respective pools
    void cleanup();
    // Round-robin worker selection
    WorkerThread* next_worker(bool force_async);

private:
    size_t CPU_cores_count_ = 0;
    size_t threads_count_ = 0;
    size_t round_robin_ = 0;
    JobSystemScheme scheme_; 
    std::vector<WorkerThread*> threads_;

	struct SharedState;
	std::shared_ptr<SharedState> ss_;
};

} // namespace kb