#include "thread/job.h"
#include "assert/assert.h"
#include "logger/logger.h"
#include "memory/heap_area.h"
#include "memory/memory.h"
#include "memory/pool_allocator.h"
#include "thread/sync.h"
#include "time/clock.h"
#include "util/sparse_set.h"

#include "atomic_queue/atomic_queue.h"
#include <algorithm>
#include <bitset>
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#define ENABLE_WORK_STEALING 1
#define PROFILING 1

namespace kb
{

struct Job
{
    JobSystem::JobFunction function = []() {};
    JobSystem::JobHandle handle;
};

static constexpr size_t k_max_jobs = 1024;
static constexpr size_t k_hnd_guard_bits = 48;
static constexpr size_t k_page_align = 64;
static constexpr size_t k_job_max_align = k_page_align - 1;
static constexpr size_t k_job_node_size = sizeof(Job) + k_job_max_align;

using HandlePool = SecureSparsePool<JobSystem::JobHandle, k_max_jobs, k_hnd_guard_bits>;
using PoolArena =
    memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>;

static constexpr bool k_minimize_contention = true;
static constexpr bool k_maximize_throughput = true;
#if ENABLE_WORK_STEALING
static constexpr bool k_SPSC = false; // Each worker can steal from another worker's queue -> SPMC
#else
static constexpr bool k_SPSC = true; // Each worker thread has its own queue, only main thread enqueues jobs
#endif

template <typename T>
using LockFreeQueue = atomic_queue::AtomicQueue<T, k_max_jobs, T{}, k_minimize_contention, k_maximize_throughput,
                                                false, // TOTAL_ORDER
                                                k_SPSC>;

struct DisplayHandle
{
    DisplayHandle(JobSystem::JobHandle handle)
    {
        guard = HandlePool::guard_value(handle);
        naked = HandlePool::unguard(handle);
    }

    friend std::ostream& operator<<(std::ostream& stream, const DisplayHandle& dh);

    size_t guard;
    size_t naked;
};

std::ostream& operator<<(std::ostream& stream, const DisplayHandle& dh)
{
    stream << dh.naked << '/' << dh.guard;
    return stream;
}

struct SharedState
{
    JobSystem* job_system;
    std::atomic<uint64_t> status = {0};
    std::atomic<bool> running = {true};
    std::condition_variable cv_wake; // To wake worker threads
    std::mutex wake_mutex;

    SpinLock handle_lock;
    PoolArena job_pool;
    HandlePool handle_pool;
};

class WorkerThread
{
public:
    WorkerThread(uint32_t tid, SharedState& ss)
        : tid_(tid), ss_(ss)
#if PROFILING
          ,
          active_time_(0), idle_time_(0)
#endif
    {
        KLOG("thread", 0) << "Worker thread #" << tid_ << " ready" << std::endl;
    }

    ~WorkerThread() { KLOG("thread", 0) << "Worker thread #" << tid_ << " destroyed" << std::endl; }

    void spawn();
    void run();
    inline void join() { thread_.join(); }
    inline uint32_t get_tid() const { return tid_; }

#if PROFILING
    inline auto get_active_time() const { return active_time_; }
    inline auto get_idle_time() const { return idle_time_; }
#endif

    inline void release_handle(JobSystem::JobHandle handle)
    {
        const std::lock_guard<SpinLock> lock(ss_.handle_lock);
        ss_.handle_pool.release(handle);
    }

    inline void enqueue(Job* job)
    {
        KLOG("thread", 0) << "T#" << tid_ << ": Enqueuing job " << DisplayHandle(job->handle) << std::endl;
        jobs_.push(job);
    }

    void cleanup();

private:
    uint32_t tid_;
    SharedState& ss_;
    std::thread thread_;

#if PROFILING
    std::chrono::microseconds active_time_;
    std::chrono::microseconds idle_time_;
#endif

    LockFreeQueue<Job*> jobs_;
    LockFreeQueue<Job*> dead_jobs_;
};

void WorkerThread::spawn() { thread_ = std::thread(&WorkerThread::run, this); }

void WorkerThread::run()
{
    auto execute = [this](Job* job) {
#if PROFILING
        microClock clk;
#endif
        auto handle = job->handle;
        KLOG("thread", 0) << "T#" << tid_ << " -> " << DisplayHandle(handle) << std::endl;
        job->function();
        release_handle(handle);
        dead_jobs_.push(job);
        ss_.status.fetch_sub(1);
        // ss_.cv_wait.notify_one();
        KLOG("thread", 0) << "T#" << tid_ << " <- " << DisplayHandle(handle) << std::endl;
#if PROFILING
        active_time_ += clk.get_elapsed_time();
#endif
    };

    while(ss_.running.load(std::memory_order_acquire))
    {
        // Get a job you lazy bastard
        if(!jobs_.was_empty())
        {
            auto* job = jobs_.pop();
            execute(job);
        }
        else
        {
#if ENABLE_WORK_STEALING
            // Try to steal a job
            auto* worker = ss_.job_system->random_worker();
            if(worker->tid_ != tid_)
            {
                Job* job = nullptr;
                if(worker->jobs_.try_pop(job))
                {
                    KLOG("thread", 0) << "T#" << tid_ << " stole job " << DisplayHandle(job->handle) << " from T#"
                                      << worker->tid_ << std::endl;
                    execute(job);
                    continue;
                }
            }
#endif

#if PROFILING
            microClock clk;
#endif
            // TODO: Job stealing
            std::unique_lock<std::mutex> lck(ss_.wake_mutex);
            ss_.cv_wake.wait(lck); // Spurious wakeups have no effect because we check if the queue is empty
#if PROFILING
            idle_time_ += clk.get_elapsed_time();
#endif
        }
    }
}

void WorkerThread::cleanup()
{
    while(!dead_jobs_.was_empty())
    {
        Job* job = dead_jobs_.pop();
        K_DELETE(job, ss_.job_pool);
    }
}

struct JobSystem::Storage
{
    size_t CPU_cores_count = 0;
    size_t threads_count = 0;
    size_t selected_worker = 0;
    std::random_device rd;
    std::vector<WorkerThread*> threads;
    SharedState ss;
};

JobSystem::JobSystem(memory::HeapArea& area) : storage_(std::make_shared<Storage>())
{
    // Find the number of CPU cores
    storage_->CPU_cores_count = std::thread::hardware_concurrency();
    // Main thread already takes one core
    storage_->threads_count = std::max(1ul, storage_->CPU_cores_count - 1ul);

    // Spawn workers
    KLOGN("thread") << "JobSystem: spawning worker threads." << std::endl;
    KLOG("thread", 0) << "Detected " << WCC('v') << storage_->CPU_cores_count << WCC(0) << " CPU cores." << std::endl;
    KLOG("thread", 0) << "Spawning " << WCC('v') << storage_->threads_count << WCC(0) << " worker threads."
                      << std::endl;

    storage_->ss.job_system = this;
    storage_->ss.job_pool.init(area, k_job_node_size + PoolArena::DECORATION_SIZE, k_max_jobs, "JobPool");

    storage_->threads.reserve(storage_->threads_count);
    for(uint32_t ii = 0; ii < storage_->threads_count; ++ii)
        storage_->threads.push_back(new WorkerThread(ii, storage_->ss));
    // Thread spawning is delayed to avoid a race condition of run() with tha atomic queue's ctor on memset
    for(auto* thd : storage_->threads)
        thd->spawn();

    KLOGI << "done" << std::endl;
}

JobSystem::~JobSystem() { shutdown(); }

void JobSystem::shutdown()
{
    KLOGN("thread") << "Shutting down job system." << std::endl;
    KLOG("thread", 0) << "Waiting for jobs to finish." << std::endl;
    wait();
    KLOG("thread", 0) << "All threads are joinable." << std::endl;

    cleanup();

    // Notify all threads they are going to die
    storage_->ss.running.store(false, std::memory_order_release);
    storage_->ss.cv_wake.notify_all();
    for(auto* thd : storage_->threads)
        thd->join();

#if PROFILING
    for(auto* thd : storage_->threads)
    {
        KLOG("thread", 1) << "Thread #" << thd->get_tid() << std::endl;
        KLOGI << "Active: " << thd->get_active_time().count() << "us" << std::endl;
        KLOGI << "Idle:   " << thd->get_idle_time().count() << "us" << std::endl;
    }
#endif

    for(auto* thd : storage_->threads)
        delete thd;

    KLOG("thread", 1) << "done." << std::endl;
}

void JobSystem::cleanup()
{
    for(auto* thd : storage_->threads)
        thd->cleanup();
}

JobSystem::JobHandle JobSystem::schedule(JobFunction function)
{
    JobHandle handle;
    {
        const std::lock_guard<SpinLock> lock(storage_->ss.handle_lock);
        handle = storage_->ss.handle_pool.acquire();
    }

    // TMP: Round-robin worker selection
    Job* job = K_NEW_ALIGN(Job, storage_->ss.job_pool, k_page_align){function, handle};
    storage_->ss.status.fetch_add(1);
    storage_->threads[storage_->selected_worker]->enqueue(job);
    storage_->selected_worker = (storage_->selected_worker + 1) % storage_->threads_count;

    return handle;
}

void JobSystem::update()
{
    cleanup();
    // Wake all worker threads
    storage_->ss.cv_wake.notify_all();
}

bool JobSystem::is_busy() { return storage_->ss.status.load(std::memory_order_relaxed) > 0; }

bool JobSystem::is_work_done(JobHandle handle) { return !storage_->ss.handle_pool.is_valid(handle); }

void JobSystem::wait()
{
    // Main thread atomically increments status each time a job is pushed to the queue.
    // Worker threads atomically decrement status each time they finished a job.
    // Then we just need to wait for status to return to zero in order
    // to be sure all worker threads have finished.

    // BUG: Can deadlock (lost wakeups?)
    // std::unique_lock<std::mutex> lock(storage_->ss.wait_mutex);
    // storage_->ss.cv_wait.wait(lock, [this]() { return !is_busy(); });

    // TMP FIX: Use polling
    while(is_busy())
    {
        // Poll
        storage_->ss.cv_wake.notify_all(); // wake one worker thread
        std::this_thread::yield();         // allow this thread to be rescheduled
    }
}

WorkerThread* JobSystem::random_worker() const
{
    std::uniform_int_distribution<size_t> dist(0, storage_->threads_count - 1);
    return storage_->threads[dist(storage_->rd)];
}

} // namespace kb