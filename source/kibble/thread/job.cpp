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

// Allow idle workers to steal jobs from their siblings
#define ENABLE_WORK_STEALING 1
// Allow main thread to share some load with workers
#define ENABLE_FOREGROUND_WORK 1
// Attempt at reducing false sharing in shared state by page-aligning its members (TODO: measure this)
#define ENABLE_SHARED_STATE_PAGE_ALIGN 0
// Brief idle/active timing stats per worker thread
#define PROFILING 0

namespace kb
{

struct Job
{
    JobSystem::JobFunction function = []() {};
    JobSystem::JobHandle handle;
};

// Maximum allowable number of worker threads
static constexpr size_t k_max_threads = 32;
// Maximum number of jobs per worker thread queue
static constexpr size_t k_max_jobs = 1024;
// Number of guard bits in a JobHandle
static constexpr size_t k_hnd_guard_bits = 48;
// Size of a cache line -> controlling alignment prevents false sharing
static constexpr size_t k_cache_line_size = 64;
// Maximal padding of a Job structure within the job pool
static constexpr size_t k_job_max_align = k_cache_line_size - 1;
// Total size of a Job node inside the pool
static constexpr size_t k_job_node_size = sizeof(Job) + k_job_max_align;

#if ENABLE_SHARED_STATE_PAGE_ALIGN
#define PAGE_ALIGN alignas(k_cache_line_size)
#else
#define PAGE_ALIGN
#endif

using HandlePool = SecureSparsePool<JobSystem::JobHandle, k_max_jobs, k_hnd_guard_bits>;
using PoolArena =
    memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>;

// Job queue configuration
#if ENABLE_WORK_STEALING
static constexpr bool k_SPSC = false; // Each worker can steal from another worker's queue -> SPMC
#else
static constexpr bool k_SPSC = true; // Each worker thread has its own queue, only main thread enqueues jobs
#endif

template <typename T> using JobQueue = atomic_queue::AtomicQueue<T, k_max_jobs, T{}, true, true, false, k_SPSC>;
template <typename T> using DeadJobQueue = atomic_queue::AtomicQueue<T, k_max_jobs, T{}, true, true, false, true>;

// Helper struct to show a handle composition
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

// Data common to all worker threads
struct JobSystem::SharedState
{
    PAGE_ALIGN std::array<WorkerThread*, k_max_threads> workers;
    PAGE_ALIGN size_t workers_count = 0;
    PAGE_ALIGN std::atomic<uint64_t> status = {0};
    PAGE_ALIGN std::atomic<bool> running = {true};
    PAGE_ALIGN PoolArena job_pool;
    PAGE_ALIGN HandlePool handle_pool;
    PAGE_ALIGN std::condition_variable cv_wake; // To wake worker threads
    PAGE_ALIGN std::mutex wake_mutex;
    PAGE_ALIGN SpinLock handle_lock;
};

class WorkerThread
{
public:
    WorkerThread(uint32_t tid, bool background, JobSystem::SharedState& ss)
        : tid_(tid), background_(background), ss_(ss), dist_(0, ss_.workers_count - 1)
#if PROFILING
          ,
          active_time_(0), idle_time_(0)
#endif
    {}

    ~WorkerThread()
    { /*KLOG("thread", 0) << "Worker thread #" << tid_ << " destroyed" << std::endl;*/
    }

    inline void spawn()
    {
        if(background_)
            thread_ = std::thread(&WorkerThread::run, this);
    }
    inline void join()
    {
        if(background_)
            thread_.join();
    }
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

    inline void submit(Job* job)
    {
        // KLOG("thread", 0) << "T#" << tid_ << ": Enqueuing job " << DisplayHandle(job->handle) << std::endl;
        jobs_.push(job);
    }

    inline auto* random_worker()
    {
        size_t idx = dist_(rd_);
        while(idx == tid_)
            idx = dist_(rd_);

        return ss_.workers[idx];
    }

    inline bool try_steal(Job*& job) { return random_worker()->jobs_.try_pop(job); }

    void execute(Job* job);
    void run();
    void foreground_work();
    void cleanup();

private:
    uint32_t tid_;
    bool background_;
    JobSystem::SharedState& ss_;
    std::thread thread_;
    std::random_device rd_;
    std::uniform_int_distribution<size_t> dist_;

#if PROFILING
    std::chrono::microseconds active_time_;
    std::chrono::microseconds idle_time_;
#endif

    JobQueue<Job*> jobs_;
    DeadJobQueue<Job*> dead_jobs_;
};

void WorkerThread::execute(Job* job)
{
#if PROFILING
    microClock clk;
#endif
    auto handle = job->handle;
    // KLOG("thread", 0) << "T#" << tid_ << " -> " << DisplayHandle(handle) << std::endl;
    job->function();
    release_handle(handle);
    dead_jobs_.push(job);
    ss_.status.fetch_sub(1);
    // ss_.cv_wait.notify_one();
    // KLOG("thread", 0) << "T#" << tid_ << " <- " << DisplayHandle(handle) << std::endl;
#if PROFILING
    active_time_ += clk.get_elapsed_time();
#endif
}

void WorkerThread::run()
{
    // KLOG("thread", 0) << "Worker thread #" << tid_ << " ready" << std::endl;

    while(ss_.running.load(std::memory_order_acquire))
    {
        // Get a job you lazy bastard
        Job* job = nullptr;
        if(jobs_.try_pop(job))
            execute(job);
        else
        {
#if ENABLE_WORK_STEALING
            // Try to steal a job
            if(try_steal(job))
            {
                execute(job);
                continue;
            }
#endif

#if PROFILING
            microClock clk;
#endif
            std::unique_lock<std::mutex> lck(ss_.wake_mutex);
            ss_.cv_wake.wait(lck); // Spurious wakeups have no effect because we check if the queue is empty
#if PROFILING
            idle_time_ += clk.get_elapsed_time();
#endif
        }
    }
}

void WorkerThread::foreground_work()
{
    K_ASSERT(!background_, "foreground_work() should not be called in a background thread.");
    Job* job = nullptr;
    while(jobs_.try_pop(job))
        execute(job);
    /*while(try_steal(job))
        execute(job);*/
}

void WorkerThread::cleanup()
{
    // Return all finished jobs to the pool
    Job* job = nullptr;
    while(dead_jobs_.try_pop(job))
    {
        K_DELETE(job, ss_.job_pool);
    }
}

JobSystem::JobSystem(memory::HeapArea& area) : ss_(std::make_shared<SharedState>())
{
    // Find the number of CPU cores
    CPU_cores_count_ = std::thread::hardware_concurrency();

#if ENABLE_FOREGROUND_WORK
    threads_count_ = std::min(k_max_threads, std::max(1ul, CPU_cores_count_));
#else
    // Main thread already takes one core
    threads_count_ = std::min(k_max_threads, std::max(1ul, CPU_cores_count_ - 1ul));
#endif

    // Spawn workers
    KLOGN("thread") << "[JobSystem] Spawning worker threads." << std::endl;
    KLOGI << "Detected " << WCC('v') << CPU_cores_count_ << WCC(0) << " CPU cores." << std::endl;
    KLOGI << "Spawning " << WCC('v') << threads_count_ << WCC(0) << " worker threads." << std::endl;
#if ENABLE_FOREGROUND_WORK
    KLOGI << "Worker " << WCC('v') << 0 << WCC(0) << " is foreground." << std::endl;
#endif

    ss_->workers.fill(nullptr);
    ss_->workers_count = threads_count_;
    ss_->job_pool.init(area, k_job_node_size + PoolArena::DECORATION_SIZE, k_max_jobs, "JobPool");

    threads_.reserve(threads_count_);
    for(uint32_t ii = 0; ii < threads_count_; ++ii)
    {
        auto* worker = new WorkerThread(ii, (ii != 0) || (ENABLE_FOREGROUND_WORK == 0), *ss_);
        threads_.push_back(worker);
        ss_->workers[ii] = worker;
    }
    // Thread spawning is delayed to avoid a race condition of run() with tha atomic queue's ctor on memset
    for(auto* thd : threads_)
        thd->spawn();

    KLOGG("thread") << "JobSystem ready." << std::endl;
}

JobSystem::~JobSystem() { shutdown(); }

void JobSystem::shutdown()
{
    KLOGN("thread") << "[JobSystem] Shutting down." << std::endl;
    KLOGI << "Waiting for jobs to finish." << std::endl;
    wait();
    KLOGI << "All threads are joinable." << std::endl;

    cleanup();

    // Notify all threads they are going to die
    ss_->running.store(false, std::memory_order_release);
    ss_->cv_wake.notify_all();
    for(auto* thd : threads_)
        thd->join();

#if PROFILING
    for(auto* thd : threads_)
    {
        KLOG("thread", 1) << "Thread #" << thd->get_tid() << std::endl;
        KLOGI << "Active: " << thd->get_active_time().count() << "us" << std::endl;
        KLOGI << "Idle:   " << thd->get_idle_time().count() << "us" << std::endl;
    }
#endif

    for(auto* thd : threads_)
        delete thd;

    KLOGG("thread") << "JobSystem shutdown complete." << std::endl;
}

void JobSystem::cleanup()
{
    for(auto* thd : threads_)
        thd->cleanup();
}

JobSystem::JobHandle JobSystem::schedule(JobFunction function)
{
    JobHandle handle;
    {
        const std::lock_guard<SpinLock> lock(ss_->handle_lock);
        handle = ss_->handle_pool.acquire();
    }

    // TMP: Round-robin worker selection
    Job* job = K_NEW_ALIGN(Job, ss_->job_pool, k_cache_line_size){function, handle};
    ss_->status.fetch_add(1);
    threads_[round_robin_]->submit(job);
    round_robin_ = (round_robin_ + 1) % threads_count_;

    return handle;
}

void JobSystem::update()
{
    cleanup();
#if ENABLE_FOREGROUND_WORK
    threads_[0]->foreground_work();
#endif
    // Wake all worker threads
    ss_->cv_wake.notify_all();
}

bool JobSystem::is_busy() { return ss_->status.load(std::memory_order_relaxed) > 0; }

bool JobSystem::is_work_done(JobHandle handle) { return !ss_->handle_pool.is_valid(handle); }

void JobSystem::wait()
{
    // Main thread atomically increments status each time a job is pushed to the queue.
    // Worker threads atomically decrement status each time they finished a job.
    // Then we just need to wait for status to return to zero in order
    // to be sure all worker threads have finished.

    // BUG: Can deadlock (lost wakeups?)
    // std::unique_lock<std::mutex> lock(ss_->wait_mutex);
    // ss_->cv_wait.wait(lock, [this]() { return !is_busy(); });

    // TMP FIX: Use polling
    while(is_busy())
    {
        // Poll
        ss_->cv_wake.notify_all(); // wake worker threads
        std::this_thread::yield(); // allow this thread to be rescheduled
    }
}

} // namespace kb