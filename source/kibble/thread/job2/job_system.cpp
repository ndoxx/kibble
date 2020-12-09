#include "thread/job2/job_system.h"
#include "logger/logger.h"
#include "thread/job2/impl/worker.h"

#include <thread>

namespace kb
{
namespace th2
{

void Job::add_child(Job* child)
{
    dependants.push_back(child);
    child->dependency_count.fetch_add(1);
}

JobSystem::JobSystem(memory::HeapArea& area, const JobSystemScheme& scheme)
    : scheme_(scheme), ss_(std::make_shared<SharedState>())
{
    KLOGN("thread") << "[JobSystem] Initializing." << std::endl;

    // Log scheme
    KLOG("thread", 0) << "Detail:" << std::endl;
    KLOGI << "Work stealing: " << (scheme_.enable_work_stealing ? "enabled" : "disabled") << std::endl;

    // Find the number of CPU cores
    CPU_cores_count_ = std::thread::hardware_concurrency();
    // Select worker count based on scheme and CPU cores
    size_t max_threads = (scheme_.max_workers != 0) ? std::min(k_max_threads, scheme_.max_workers + 1) : k_max_threads;
    threads_count_ = std::min(max_threads, CPU_cores_count_);

    // TODO: create monitor

    // TODO: create scheduler

    // TODO: init job pool
    (void)area;

    // Spawn threads_count_-1 workers
    KLOGI << "Detected " << WCC('v') << CPU_cores_count_ << WCC(0) << " CPU cores." << std::endl;
    KLOGI << "Spawning " << WCC('v') << threads_count_ - 1 << WCC(0) << " worker threads." << std::endl;

    workers_.resize(threads_count_);
    for(uint32_t ii = 0; ii < threads_count_; ++ii)
    {
        // TODO: Use K_NEW
        WorkerDescriptor desc;
        desc.is_background = (ii != 0);
        desc.can_steal = (ii != 0) && scheme_.enable_work_stealing;
        desc.tid = ii;
        workers_[ii] = new WorkerThread(desc, *this);
    }
    // Thread spawning is delayed to avoid a race condition of run() with other workers ctors
    for(auto* worker : workers_)
        worker->spawn();

    KLOGG("thread") << "[JobSystem] Ready." << std::endl;
}

JobSystem::~JobSystem() { shutdown(); }

void JobSystem::shutdown()
{
    KLOGN("thread") << "[JobSystem] Shutting down." << std::endl;
    KLOGI << "Waiting for jobs to finish." << std::endl;
    // wait();
    KLOGI << "All threads are joinable." << std::endl;
    // Notify all threads they are going to die
    ss_->running.store(false, std::memory_order_release);
    ss_->cv_wake.notify_all();
    for(auto* worker : workers_)
        worker->join();

    for(auto* worker : workers_)
        delete worker;

    KLOGG("thread") << "[JobSystem] Shutdown complete." << std::endl;
}

/*
void JobSystem::use_persistence_file(const fs::path& filepath)
{

}



Job* JobSystem::create_job(JobKernel&& kernel, const JobMetadata& meta)
{

}

void JobSystem::schedule(Job* job)
{

}
*/

} // namespace th2
} // namespace kb