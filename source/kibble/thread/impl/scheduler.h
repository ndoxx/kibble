#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace kb
{

class JobSystem;
class WorkerThread;
struct Job;
class Scheduler
{
public:
    Scheduler(JobSystem& js);
    virtual ~Scheduler() = default;
    void schedule(Job* job);
    virtual void submit() = 0;
    virtual bool is_dynamic() { return false; }

protected:
    JobSystem& js_;
    std::vector<Job*> scheduled_jobs_;
};

class RoundRobinScheduler : public Scheduler
{
public:
    RoundRobinScheduler(JobSystem& js);
    virtual ~RoundRobinScheduler() = default;
    virtual void submit() override;

private:
    std::size_t round_robin_ = 0;
};

class MininmumLoadScheduler : public Scheduler
{
public:
    MininmumLoadScheduler(JobSystem& js);
    virtual ~MininmumLoadScheduler() = default;
    virtual void submit() override;
    virtual bool is_dynamic() override { return true; }
private:
    std::size_t round_robin_ = 0;
};

} // namespace kb