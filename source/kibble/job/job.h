#pragma once

#include <functional>
#include <memory>

namespace kb
{
namespace memory
{
class HeapArea;
}

class JobSystem
{
public:
    using JobFunction = std::function<void(void)>;
    using JobHandle = size_t;

    JobSystem(memory::HeapArea& area);
    ~JobSystem();

    // Wait for all jobs to finish, join worker threads and destroy system storage
    void shutdown();
    // Enqueue a new job for asynchronous execution and return a handle for this job
    JobHandle schedule(JobFunction function);
    // Hold execution on this thread till all jobs are processed
    void wait();
    // Non-blockingly check if any worker threads are busy
    bool is_busy();
    // Non-blockingly check if a job is processed
    bool is_work_done(JobHandle handle);
    // Call this regularly, all scheduled tasks will be performed
    void update();

private:
    // Return dead jobs to their respective pools
    void cleanup();

private:
	friend class WorkerThread;
	struct Storage;
	std::shared_ptr<Storage> storage_;
};

} // namespace kb