#pragma once

#include "thread/impl/common.h"
#include "thread/sync.h"
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

namespace kb
{

struct JobMetadata
{
    uint64_t label = 0;
    int64_t execution_time_us = 0;
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

    WorkerThread(uint32_t tid, bool background, bool can_steal, JobSystem& js);

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
    inline auto get_active_time() const { return active_time_us_; }
    inline auto get_idle_time() const { return idle_time_us_; }
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

    void execute(Job* job);
    void run();
    void foreground_work();
    WorkerThread* random_worker();

private:
    uint32_t tid_;
    bool background_;
    bool can_steal_;
    std::atomic<State> state_;
    JobSystem& js_;
    SharedState& ss_;
    std::thread thread_;
    std::random_device rd_;
    std::uniform_int_distribution<size_t> dist_;

#if PROFILING
    int64_t active_time_us_;
    int64_t idle_time_us_;
#endif

    JobQueue<Job*> jobs_;
};

} // namespace kb