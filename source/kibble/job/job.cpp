#include "job/job.h"

#include "assert/assert.h"
#include "logger/logger.h"
#include "memory/heap_area.h"
#include "memory/memory.h"
#include "memory/pool_allocator.h"
#include "util/sparse_set.h"

#include "atomic_queue/atomic_queue.h"
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace kb
{

struct Job
{
    JobSystem::JobFunction function = []() {};
    JobSystem::JobHandle handle;
    // JobSystem::JobHandle parent;
    bool alive = true;
};

class WorkerThread
{
public:
    WorkerThread(uint32_t tid, JobSystem::Storage& storage)
        : tid_(tid), storage_(storage), thread_(&WorkerThread::run, this)
    {
        (void)tid_;
    }

    void run();
    inline void join() { thread_.join(); }

private:
    uint32_t tid_;
    JobSystem::Storage& storage_;
    std::thread thread_;
    // std::vector<Job> wait_list_; // To store jobs that have unmet dependencies
};

static constexpr size_t k_max_jobs = 256;
static constexpr size_t k_hnd_guard_bits = 16;
static constexpr size_t k_page_align = 64;
static constexpr size_t k_job_max_align = k_page_align - 1;
static constexpr size_t k_job_node_size = sizeof(Job) + k_job_max_align;
static constexpr bool k_minimize_contention = true;
static constexpr bool k_maximize_throughput = true;
static constexpr bool k_SPSC = false;

using HandlePool = SecureSparsePool<JobSystem::JobHandle, k_max_jobs, k_hnd_guard_bits>;
using PoolArena =
    memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>;
template <typename T>
using AtomicQueue = atomic_queue::AtomicQueue<T, k_max_jobs, T{}, k_minimize_contention, k_maximize_throughput,
                                              false, // TOTAL_ORDER
                                              k_SPSC>;

#define DHND_( X ) HandlePool::unguard( X ) << '/' << HandlePool::guard_value( X )

struct JobSystem::Storage
{
    Storage(memory::HeapArea& area)
        : job_pool(area, k_job_node_size + PoolArena::DECORATION_SIZE, k_max_jobs, "JobPool")
    {}

    size_t CPU_cores_count = 0;
    size_t threads_count = 0;
    uint64_t current_status = 0;
    std::atomic<uint64_t> status;
    std::atomic<bool> running;
    std::condition_variable cv_wake; // To wake worker threads
    std::condition_variable cv_wait; // For the main thread to wait on
    std::mutex wake_mutex;
    std::mutex wait_mutex;
    std::vector<WorkerThread> threads;
    std::vector<Job*> alive_jobs;

    PoolArena job_pool;
    HandlePool handle_pool;
    AtomicQueue<Job*> jobs;
};

void WorkerThread::run()
{
    while(storage_.running.load(std::memory_order_acquire))
    {
        // Get a job you lazy bastard
        if(!storage_.jobs.was_empty())
        {
            Job* job = storage_.jobs.pop();

            // Check dependencies
            /*if(job->parent.is_valid())
            {
                // Parent job has not finished yet, push the job into the wait list
                wait_list_.push_back(job);
                continue;
            }*/

            auto handle = job->handle;
            KLOG("thread", 1) << 'T' << tid_ << " took job  " << DHND_(handle) << std::endl;
            job->function();
            storage_.handle_pool.release(handle);
            job->alive = false;
            K_DELETE(job, storage_.job_pool);
            // Notify main thread that the job is done
            storage_.status.fetch_add(1, std::memory_order_relaxed);
            storage_.cv_wait.notify_one();
            KLOG("thread", 1) << 'T' << tid_ << " ended job " << DHND_(handle) << std::endl;
        }
        else // No job -> wait
        {
            std::unique_lock<std::mutex> lck(storage_.wake_mutex);
            storage_.cv_wake.wait(lck); // Spurious wakeups have no effect because we check if the queue is empty
        }
    }
}

JobSystem::JobSystem(memory::HeapArea& area) : storage_(std::make_shared<Storage>(area))
{
    // Find the number of CPU cores
    storage_->CPU_cores_count = std::thread::hardware_concurrency();
    // Main thread already takes one core
    storage_->threads_count = storage_->CPU_cores_count - 1;
}

JobSystem::~JobSystem() { shutdown(); }

void JobSystem::spawn_workers()
{
    KLOGN("thread") << "JobSystem: spawning worker threads." << std::endl;
    KLOG("thread", 0) << "Detected " << WCC('v') << storage_->CPU_cores_count << WCC(0) << " CPU cores." << std::endl;
    KLOG("thread", 0) << "Spawning " << WCC('v') << storage_->threads_count << WCC(0) << " worker threads."
                      << std::endl;

    storage_->running.store(true, std::memory_order_release);
    storage_->status.store(0, std::memory_order_release);
    storage_->current_status = 0;
    storage_->threads.reserve(storage_->threads_count); // Avoids moving the workers later on
    for(uint32_t ii = 0; ii < storage_->threads_count; ++ii)
        storage_->threads.emplace_back(ii, *storage_);

    KLOGI << "done" << std::endl;
}

void JobSystem::shutdown()
{
    KLOGN("thread") << "Shutting down job system." << std::endl;
    KLOG("thread", 1) << "Waiting for jobs to finish." << std::endl;
    wait();
    KLOG("thread", 1) << "All threads are joinable." << std::endl;

    // Notify all threads they are going to die
    storage_->running.store(false, std::memory_order_release);
    storage_->cv_wake.notify_all();
    for(auto&& thd : storage_->threads)
        thd.join();

    KLOG("thread", 1) << "done." << std::endl;
}

/*
void JobSystem::cleanup()
{
    // Return dead jobs to the pool
    std::remove_if(storage_->alive_jobs.begin(), storage_->alive_jobs.end(), [this](Job* job) {
        if(!job->alive)
        {
            K_DELETE(job, storage_->job_pool);
            return true;
        }
        return false;
    });
}
*/

JobSystem::JobHandle JobSystem::schedule(JobFunction function)
{
    JobHandle handle = storage_->handle_pool.acquire();
    ++storage_->current_status;

    // Enqueue new job
    KLOG("thread", 1) << "Enqueuing job " << DHND_(handle) << std::endl;

    Job* job = K_NEW_ALIGN(Job, storage_->job_pool, k_page_align){function, handle};
    storage_->jobs.push(job);

    return handle;
}

void JobSystem::update()
{
    // Wake all worker threads
    storage_->cv_wake.notify_all();
}

bool JobSystem::is_busy() { return storage_->status.load(std::memory_order_relaxed) < storage_->current_status; }

bool JobSystem::is_work_done(JobHandle handle) { return !storage_->handle_pool.is_valid(handle); }

void JobSystem::wait()
{
    // Main thread increments current_status each time a job is pushed to the queue.
    // Worker threads atomically increment status each time they finished a job.
    // Then we just need to wait for status to catch up with current_status in order
    // to be sure all worker threads have finished.
    std::unique_lock<std::mutex> lock(storage_->wait_mutex);
    storage_->cv_wait.wait(lock, [this]() { return !is_busy(); });
}

void JobSystem::wait_for(JobHandle handle)
{
    // Same as before but we wait for the passed handle to be invalidated by a worker thread.
    std::unique_lock<std::mutex> lock(storage_->wait_mutex);
    storage_->cv_wait.wait(lock, [&handle, this]() { return !storage_->handle_pool.is_valid(handle); });
}

} // namespace kb