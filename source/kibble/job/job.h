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
    using JobHandle = uint32_t;

    JobSystem(memory::HeapArea& area);
    ~JobSystem();

    // Spawn worker threads
    void spawn_workers();
    // Wait for all jobs to finish, join worker threads and destroy system storage
    void shutdown();
    // Enqueue a new job for asynchronous execution and return a handle for this job
    // This should not be used for jobs with dependencies
    JobHandle schedule(JobFunction function);
    // Non-blockingly check if any worker threads are busy
    bool is_busy();
    // Non-blockingly check if a job is processed
    bool is_work_done(JobHandle handle);
    // Hold execution on this thread till all jobs are processed
    void wait();
    // Hold execution on this thread till a particular job is processed
    void wait_for(JobHandle handle);

    // Call this regularly, all scheduled tasks will be performed
    void update();

private:
	friend class WorkerThread;
	struct Storage;
	std::shared_ptr<Storage> storage_;
};

} // namespace kb