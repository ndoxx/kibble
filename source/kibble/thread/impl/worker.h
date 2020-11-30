#pragma once

#include "thread/impl/common.h"
#include "thread/sync.h"
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

namespace kb
{

enum class SchedulerExecutionPolicy : uint8_t
{
    automatic, // Job may be executed synchronously during wait() or asynchronously
    deferred,  // Job execution is synchronous and deferred to the next wait() call
    async      // Job will be executed asynchronously
};

struct JobMetadata
{
    uint64_t label = 0;
    int64_t execution_time_us = 0;
    SchedulerExecutionPolicy execution_policy;
};

struct Job
{
    JobFunction function = []() {};
    JobHandle handle;
    JobMetadata metadata;
};

class WorkerThread;
// Data common to all worker threads
struct SharedState
{
    PAGE_ALIGN std::atomic<uint64_t> pending = {0};
    PAGE_ALIGN std::atomic<bool> running = {true};
    PAGE_ALIGN PoolArena job_pool;
    PAGE_ALIGN HandlePool handle_pool;
    PAGE_ALIGN std::condition_variable cv_wake; // To wake worker threads
    PAGE_ALIGN std::mutex wake_mutex;
    PAGE_ALIGN SpinLock handle_lock;
    PAGE_ALIGN DeadJobQueue<Job*> dead_jobs;
};

class JobSystem;
class WorkerThread
{
public:
    enum class State : uint8_t
    {
        Idle,
        Running,
        Stopping
    };

    WorkerThread(tid_t tid, bool background, bool can_steal, JobSystem& js);

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
    inline tid_t get_tid() const { return tid_; }

#if PROFILING
    inline const auto& get_activity() const { return activity_; }
    inline auto& get_activity() { return activity_; }
#endif

    inline void release_handle(JobHandle handle)
    {
        const std::scoped_lock<SpinLock> lock(ss_.handle_lock);
        ss_.handle_pool.release(handle);
    }
    inline void submit(Job* job)
    {
        ANNOTATE_HAPPENS_BEFORE(&jobs_); // Avoid false positives with TSan
        jobs_.push(job);
    }
    inline bool try_steal(Job*& job)
    {
        auto* w = random_worker();
        ANNOTATE_HAPPENS_AFTER(&w->jobs_); // Avoid false positives with TSan
        return w->jobs_.try_pop(job);
    }
    inline State get_state() const { return state_.load(std::memory_order_relaxed); }
    inline size_t get_queue_size() const { return jobs_.was_size(); }
    
    void execute(Job* job);
    void run();
    bool foreground_work();
    WorkerThread* random_worker();

private:
    tid_t tid_;
    bool background_;
    bool can_steal_;
    std::atomic<State> state_;
    JobSystem& js_;
    SharedState& ss_;
    std::thread thread_;
    std::random_device rd_;
    std::uniform_int_distribution<size_t> dist_;

#if PROFILING
    WorkerActivity activity_;
#endif

    JobQueue<Job*> jobs_;
};

} // namespace kb