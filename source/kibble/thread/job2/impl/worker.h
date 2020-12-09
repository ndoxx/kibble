#pragma once
#include "thread/job2/impl/common.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace kb
{
namespace th2
{

struct WorkerDescriptor
{
    bool is_background = false;
    bool can_steal = false;
    tid_t tid;
};

struct Job;
// Data common to all worker threads
struct SharedState
{
    PAGE_ALIGN std::atomic<uint64_t> pending = {0}; // Number of tasks left
    PAGE_ALIGN std::atomic<bool> running = {true};  // Flag to signal workers when they should stop and join
    PAGE_ALIGN std::condition_variable cv_wake;     // To wake worker threads
    PAGE_ALIGN std::mutex wake_mutex;               // Workers wait on this one when they're idle
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

    WorkerThread(const WorkerDescriptor&, JobSystem&);

    void spawn();
    void join();

private:
    void run();
    bool get_job(Job*& job);

private:
    tid_t tid_;
    bool can_steal_;
    bool is_background_;
    JobSystem& js_;
    SharedState& ss_;
    std::atomic<State> state_;
    std::thread thread_;
    JobQueue<Job*> jobs_;
};

} // namespace th2
} // namespace kb