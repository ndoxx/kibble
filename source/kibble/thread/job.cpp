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

// Attempt at reducing false sharing in shared state by page-aligning its members (TODO: measure this)
#define ENABLE_SHARED_STATE_PAGE_ALIGN 0
// Brief idle/active timing stats per worker thread
#define PROFILING 1

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

template <typename T> using JobQueue = atomic_queue::AtomicQueue<T, k_max_jobs, T{}, true, true, false, false>;
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
    PAGE_ALIGN std::atomic<uint64_t> pending = {0};
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
    enum class State : uint8_t
    {
        Idle,
        Running,
        Stopping
    };

    WorkerThread(uint32_t tid, bool background, bool can_steal, JobSystem::SharedState& ss)
        : tid_(tid), background_(background), can_steal_(can_steal), ss_(ss), dist_(0, ss_.workers_count - 1)
#if PROFILING
          ,
          active_time_(0), idle_time_(0)
#endif
    {}

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
        const std::scoped_lock<SpinLock> lock(ss_.handle_lock);
        ss_.handle_pool.release(handle);
    }

    inline void submit(Job* job) { jobs_.push(job); }

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
    bool can_steal_;
    std::atomic<State> state_;
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
    job->function();
    release_handle(handle);
    dead_jobs_.push(job);
    ss_.pending.fetch_sub(1);
#if PROFILING
    active_time_ += clk.get_elapsed_time();
#endif
}

void WorkerThread::run()
{
    K_ASSERT(background_, "run() should not be called in the main thread.");

    while(ss_.running.load(std::memory_order_acquire))
    {
        state_.store(State::Running, std::memory_order_release);

        // Get a job you lazy bastard
        Job* job = nullptr;
        if(jobs_.try_pop(job))
            execute(job);
        else
        {
            // Try to steal a job
            if(can_steal_ && try_steal(job))
            {
                execute(job);
                continue;
            }

            state_.store(State::Idle, std::memory_order_release);
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

    state_.store(State::Stopping, std::memory_order_release);
}

void WorkerThread::foreground_work()
{
    K_ASSERT(!background_, "foreground_work() should not be called in a background thread.");
    Job* job = nullptr;
    if(jobs_.try_pop(job))
        execute(job);
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

JobSystem::JobSystem(memory::HeapArea& area, const JobSystemScheme& scheme)
    : scheme_(scheme), ss_(std::make_shared<SharedState>())
{
    // Find the number of CPU cores
    CPU_cores_count_ = std::thread::hardware_concurrency();
    // Select worker count based on scheme and CPU cores
    if(scheme_.max_threads == 0)
        threads_count_ =
            std::min(k_max_threads, std::max(1ul, CPU_cores_count_ - size_t(!scheme_.enable_foreground_work)));
    else
        threads_count_ = std::min(k_max_threads, scheme_.max_threads + size_t(scheme_.enable_foreground_work));

    // Spawn workers
    KLOGN("thread") << "[JobSystem] Spawning worker threads." << std::endl;
    KLOGI << "Detected " << WCC('v') << CPU_cores_count_ << WCC(0) << " CPU cores." << std::endl;
    KLOGI << "Spawning " << WCC('v') << threads_count_ - size_t(scheme_.enable_foreground_work) << WCC(0)
          << " worker threads." << std::endl;
    if(scheme_.enable_foreground_work)
    {
        KLOGI << "Worker " << WCC('v') << 0 << WCC(0) << " is foreground." << std::endl;
    }

    ss_->workers.fill(nullptr);
    ss_->workers_count = threads_count_;
    ss_->job_pool.init(area, k_job_node_size + PoolArena::DECORATION_SIZE, k_max_jobs * threads_count_, "JobPool");

    threads_.reserve(threads_count_);
    for(uint32_t ii = 0; ii < threads_count_; ++ii)
    {
        auto* worker =
            new WorkerThread(ii, (ii != 0) || !scheme_.enable_foreground_work, scheme_.enable_work_stealing, *ss_);
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

WorkerThread* JobSystem::next_worker(ExecutionPolicy policy)
{
    if(scheme_.enable_foreground_work)
    {
        if(policy == ExecutionPolicy::deferred)
            return threads_[0];
        else if((policy == ExecutionPolicy::async) && round_robin_ == 0)
            round_robin_ = (round_robin_ + 1) % threads_count_;
    }
    else
    {
        K_ASSERT(policy != ExecutionPolicy::deferred, "Cannot execute job synchronously: foreground work is disabled.");
    }

    auto* worker = threads_[round_robin_];
    round_robin_ = (round_robin_ + 1) % threads_count_;
    return worker;
}

JobSystem::JobHandle JobSystem::schedule(JobFunction&& function, ExecutionPolicy policy)
{
    JobHandle handle;
    {
        const std::scoped_lock<SpinLock> lock(ss_->handle_lock);
        handle = ss_->handle_pool.acquire();
    }

    // TMP: Round-robin worker selection
    Job* job = K_NEW_ALIGN(Job, ss_->job_pool, k_cache_line_size){std::move(function), handle};
    ss_->pending.fetch_add(1);
    next_worker(policy)->submit(job);

    return handle;
}

JobSystem::JobHandle JobSystem::async(JobFunction&& function)
{
    auto handle = schedule(std::move(function), ExecutionPolicy::async);
    ss_->cv_wake.notify_one();
    return handle;
}

void JobSystem::update()
{
    cleanup();

    // Wake all worker threads
    ss_->cv_wake.notify_all();
}

// Main thread atomically increments pending each time a job is pushed to the queue.
// Worker threads atomically decrement pending each time they finished a job.
// Then we just need to wait for pending to return to zero in order
// to be sure all worker threads have finished.
bool JobSystem::is_busy() const { return ss_->pending.load(std::memory_order_relaxed) > 0; }

bool JobSystem::is_work_done(JobHandle handle) const { return !ss_->handle_pool.is_valid(handle); }

// NOTE(ndx): Instead of busy-waiting I tried
//      std::unique_lock<std::mutex> lock(ss_->wait_mutex);
//      ss_->cv_wait.wait(lock, [this]() { return !is_busy(); });
// and in WorkerThread::execute() I do this after the fetch_sub:
//      ss_.cv_wait.notify_one();
// But it deadlocks (lost wakeups?)
void JobSystem::wait_untill(std::function<bool()> condition)
{
    // Do some work
    if(scheme_.enable_foreground_work)
        while(condition())
            threads_[0]->foreground_work();
    // Poll
    while(condition())
    {
        ss_->cv_wake.notify_all(); // wake worker threads
        std::this_thread::yield(); // allow this thread to be rescheduled
    }
}

void JobSystem::wait(std::function<bool()> condition)
{
    wait_untill([this, &condition]() { return is_busy() && condition(); });
    if(!condition())
    {
        KLOG("thread", 0) << "[JobSystem] wait() exited early." << std::endl;
    }
}

void JobSystem::wait_for(JobHandle handle, std::function<bool()> condition)
{
    wait_untill([this, &condition, handle]() { return !is_work_done(handle) && condition(); });
    if(!condition())
    {
        KLOG("thread", 0) << "[JobSystem] wait_for() exited early." << std::endl;
    }
}

} // namespace kb