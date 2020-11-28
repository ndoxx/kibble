#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace kb
{

enum class SchedulerExecutionPolicy : uint8_t
{
    automatic, // Job may be executed synchronously during wait() or asynchronously
    deferred,  // Job execution is synchronous and deferred to the next wait() call
    async      // Job will be executed asynchronously
};

struct JobMetadata;
class JobSystem;
class WorkerThread;
class Scheduler
{
public:
    Scheduler(JobSystem& js);
    virtual ~Scheduler() = default;

    virtual WorkerThread* select(uint64_t label, SchedulerExecutionPolicy policy) = 0;
    virtual void report(const JobMetadata&) {}
    virtual void reset() {}

protected:
    JobSystem& js_;
};

class RoundRobinScheduler : public Scheduler
{
public:
    RoundRobinScheduler(JobSystem& js);
    virtual ~RoundRobinScheduler() = default;

    virtual WorkerThread* select(uint64_t label, SchedulerExecutionPolicy policy) override;

private:
    std::size_t round_robin_ = 0;
};

class AssociativeDynamicScheduler : public Scheduler
{
public:
    AssociativeDynamicScheduler(JobSystem& js);
    virtual ~AssociativeDynamicScheduler() = default;

    virtual WorkerThread* select(uint64_t label, SchedulerExecutionPolicy policy) override;
    virtual void report(const JobMetadata&) override;
    virtual void reset() override;

private:
    std::size_t round_robin_ = 0;
    std::map<uint64_t, int64_t> job_durations_;
    std::vector<int64_t> loads_;
};

} // namespace kb