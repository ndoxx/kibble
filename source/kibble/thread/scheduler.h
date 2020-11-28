#pragma once

#include <cstdint>

namespace kb
{

enum class SchedulerExecutionPolicy : uint8_t
{
    automatic, // Job may be executed synchronously during wait() or asynchronously
    deferred,  // Job execution is synchronous and deferred to the next wait() call
    async      // Job will be executed asynchronously
};

class JobSystem;
class WorkerThread;
class Scheduler
{
public:
    Scheduler(JobSystem& js);
    virtual ~Scheduler() = default;

    virtual WorkerThread* select(SchedulerExecutionPolicy policy) = 0;

protected:
    JobSystem& js_;
};

class RoundRobinScheduler : public Scheduler
{
public:
    RoundRobinScheduler(JobSystem& js);
    virtual ~RoundRobinScheduler() = default;

    virtual WorkerThread* select(SchedulerExecutionPolicy policy) override;

private:
    std::size_t round_robin_ = 0;
};

} // namespace kb