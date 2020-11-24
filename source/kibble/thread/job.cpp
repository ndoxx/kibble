#include "thread/job.h"
#include "thread/sync.h"

#include "assert/assert.h"
#include "logger/logger.h"
#include "memory/heap_area.h"
#include "memory/memory.h"
#include "memory/pool_allocator.h"
#include "time/clock.h"
#include "util/sparse_set.h"

#include "atomic_queue/atomic_queue.h"
#include <algorithm>
#include <bitset>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace kb
{

// #define PROFILING

struct Job
{
    JobSystem::JobFunction function = []() {};
    JobSystem::JobHandle handle;
};

static constexpr size_t k_max_jobs = 128;
static constexpr size_t k_hnd_tw_bits = 4;
static constexpr size_t k_hnd_tw_shift = sizeof(JobSystem::JobHandle) * 8u - k_hnd_tw_bits;
static constexpr JobSystem::JobHandle k_hnd_tw_mask = make_mask<JobSystem::JobHandle>(k_hnd_tw_shift);
static constexpr size_t k_hnd_guard_bits = 48 - k_hnd_tw_bits;
static constexpr size_t k_page_align = 64;
static constexpr size_t k_job_max_align = k_page_align - 1;
static constexpr size_t k_job_node_size = sizeof(Job) + k_job_max_align;
static constexpr bool k_minimize_contention = true;
static constexpr bool k_maximize_throughput = true;
static constexpr bool k_SPSC = true; // Each worker thread has its own queue, only main thread enqueues jobs

using HandlePool = SecureSparsePool<JobSystem::JobHandle, k_max_jobs, k_hnd_guard_bits>;
using PoolArena =
    memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>;
template <typename T>
using AtomicQueue = atomic_queue::AtomicQueue<T, k_max_jobs, T{}, k_minimize_contention, k_maximize_throughput,
                                              false, // TOTAL_ORDER
                                              k_SPSC>;

static inline uint32_t to_tid(JobSystem::JobHandle handle) { return (handle & k_hnd_tw_mask) >> k_hnd_tw_shift; }

static inline JobSystem::JobHandle to_pure_handle(JobSystem::JobHandle handle) { return handle & (~k_hnd_tw_mask); }

struct DisplayHandle
{
    DisplayHandle(JobSystem::JobHandle handle)
    {
        auto pure_handle = to_pure_handle(handle);
        tid = to_tid(handle);
        guard = HandlePool::guard_value(pure_handle);
        naked = HandlePool::unguard(pure_handle);
    }

    friend std::ostream& operator<<(std::ostream& stream, const DisplayHandle& dh);

    uint32_t tid;
    size_t guard;
    size_t naked;
};

std::ostream& operator<<(std::ostream& stream, const DisplayHandle& dh)
{
    stream << dh.tid << '/' << dh.naked << '/' << dh.guard;
    return stream;
}

struct SharedState
{
    std::atomic<uint64_t> status = {0};
    std::atomic<bool> running = {true};
    std::condition_variable cv_wake; // To wake worker threads
    std::mutex wake_mutex;
};

class WorkerThread
{
public:
    WorkerThread(uint32_t tid, memory::HeapArea& area, SharedState& ss)
        : tid_(tid), ss_(ss),
#ifdef PROFILING
          active_time_(0), idle_time_(0),
#endif
          job_pool_(area, k_job_node_size + PoolArena::DECORATION_SIZE, k_max_jobs, "JobPool")
    {
        KLOG("thread", 0) << "Worker thread #" << tid_ << " ready" << std::endl;
    }

    ~WorkerThread() { KLOG("thread", 0) << "Worker thread #" << tid_ << " destroyed" << std::endl; }

    void spawn();
    void run();
    inline void join() { thread_.join(); }
    inline uint32_t get_tid() const { return tid_; }

#ifdef PROFILING
    inline auto get_active_time() const { return active_time_; }
    inline auto get_idle_time() const { return idle_time_; }
#endif

    inline JobSystem::JobHandle acquire_handle()
    {
        // Synchronization is not avoidable, handle operations can (and will) be called
        // concurrently, producing garbage handles.
        // This situation should be rare enough (in my use case) and the operations
        // fast enough, for a spinlock to be a good primitive candidate.
        const std::lock_guard<SpinLock> lock(handle_lock_);
        // Add thread ID bits in front
        return handle_pool_.acquire() | (JobSystem::JobHandle(tid_) << k_hnd_tw_shift);
    }

    inline void release_handle(JobSystem::JobHandle handle)
    {
        const std::lock_guard<SpinLock> lock(handle_lock_);
        auto pure_handle = to_pure_handle(handle);
        auto tid = to_tid(handle);
        K_ASSERT(tid == tid_, "Bad thread ID, cannot release this handle");
        handle_pool_.release(pure_handle);
    }

    inline bool is_valid(JobSystem::JobHandle handle) const
    {
        auto pure_handle = to_pure_handle(handle);
        auto tid = to_tid(handle);
        return (tid == tid_ && handle_pool_.is_valid(pure_handle));
    }

    JobSystem::JobHandle enqueue(JobSystem::JobFunction&& function);
    void cleanup();

private:
    uint32_t tid_;
    SharedState& ss_;
    std::thread thread_;
    SpinLock handle_lock_;

#ifdef PROFILING
    std::chrono::microseconds active_time_;
    std::chrono::microseconds idle_time_;
#endif

    PoolArena job_pool_;
    HandlePool handle_pool_;
    AtomicQueue<Job*> jobs_;
    AtomicQueue<Job*> dead_jobs_;
};

void WorkerThread::spawn()
{
    thread_ = std::thread(&WorkerThread::run, this);
}

void WorkerThread::run()
{
    while(ss_.running.load(std::memory_order_acquire))
    {
        // Get a job you lazy bastard
        if(!jobs_.was_empty())
        {
#ifdef PROFILING
            microClock clk;
#endif
            Job* job = jobs_.pop();
            auto handle = job->handle;
            KLOG("thread", 0) << "T#" << tid_ << " -> " << DisplayHandle(handle) << std::endl;
            job->function();
            release_handle(handle);
            dead_jobs_.push(job);
            ss_.status.fetch_add(1, std::memory_order_relaxed);
            // ss_.cv_wait.notify_one();
            KLOG("thread", 0) << "T#" << tid_ << " <- " << DisplayHandle(handle) << std::endl;
#ifdef PROFILING
            active_time_ += clk.get_elapsed_time();
#endif
        }
        else // No job -> wait
        {
#ifdef PROFILING
            microClock clk;
#endif
            // TODO: Job stealing
            std::unique_lock<std::mutex> lck(ss_.wake_mutex);
            ss_.cv_wake.wait(lck); // Spurious wakeups have no effect because we check if the queue is empty
#ifdef PROFILING
            idle_time_ += clk.get_elapsed_time();
#endif
        }
    }
}

JobSystem::JobHandle WorkerThread::enqueue(JobSystem::JobFunction&& function)
{
    auto handle = acquire_handle();

    // Enqueue new job
    KLOG("thread", 0) << "T#" << tid_ << ": Enqueuing job " << DisplayHandle(handle) << std::endl;

    Job* job = K_NEW_ALIGN(Job, job_pool_, k_page_align){std::forward<JobSystem::JobFunction>(function), handle};
    jobs_.push(job);

    return handle;
}

void WorkerThread::cleanup()
{
    while(!dead_jobs_.was_empty())
    {
        Job* job = dead_jobs_.pop();
        K_DELETE(job, job_pool_);
    }
}

struct JobSystem::Storage
{
    size_t CPU_cores_count = 0;
    size_t threads_count = 0;
    uint64_t current_status = 0;
    size_t selected_worker = 0;
    std::vector<WorkerThread*> threads;
    SharedState ss;
};

JobSystem::JobSystem(memory::HeapArea& area) : storage_(std::make_shared<Storage>())
{
    // Find the number of CPU cores
    storage_->CPU_cores_count = std::thread::hardware_concurrency();
    // Main thread already takes one core
    storage_->threads_count = storage_->CPU_cores_count - 1;

    // Spawn workers
    KLOGN("thread") << "JobSystem: spawning worker threads." << std::endl;
    KLOG("thread", 0) << "Detected " << WCC('v') << storage_->CPU_cores_count << WCC(0) << " CPU cores." << std::endl;
    KLOG("thread", 0) << "Spawning " << WCC('v') << storage_->threads_count << WCC(0) << " worker threads."
                      << std::endl;

    storage_->current_status = 0;
    storage_->threads.reserve(storage_->threads_count);
    for(uint32_t ii = 0; ii < storage_->threads_count; ++ii)
        storage_->threads.push_back(new WorkerThread(ii, area, storage_->ss));
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

#ifdef PROFILING
    for(auto* thd : storage_->threads)
    {
        KLOG("thread",1) << "Thread #" << thd->get_tid() << std::endl;
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
    // TMP: Round-robin worker selection
    ++storage_->current_status;
    auto handle = storage_->threads[storage_->selected_worker]->enqueue(std::move(function));
    storage_->selected_worker = (storage_->selected_worker + 1) % storage_->threads_count;
    return handle;
}

void JobSystem::update()
{
    cleanup();
    // Wake all worker threads
    storage_->ss.cv_wake.notify_all();
}

bool JobSystem::is_busy() { return storage_->ss.status.load(std::memory_order_relaxed) < storage_->current_status; }

bool JobSystem::is_work_done(JobHandle handle)
{
    auto tid = to_tid(handle);
    K_ASSERT(tid < storage_->threads_count, "Invalid thread ID in is_work_done()");
    return !storage_->threads[tid]->is_valid(handle);
}

void JobSystem::wait()
{
    // Main thread increments current_status each time a job is pushed to the queue.
    // Worker threads atomically increment status each time they finished a job.
    // Then we just need to wait for status to catch up with current_status in order
    // to be sure all worker threads have finished.

    // BUG: Can deadlock (lost wakeups?)
    // std::unique_lock<std::mutex> lock(storage_->wait_mutex);
    // storage_->cv_wait.wait(lock, [this]() { return !is_busy(); });

    // TMP FIX: Use polling
    while(is_busy())
    {
        // Poll
        storage_->ss.cv_wake.notify_all(); // wake one worker thread
        std::this_thread::yield();         // allow this thread to be rescheduled
    }
}

} // namespace kb