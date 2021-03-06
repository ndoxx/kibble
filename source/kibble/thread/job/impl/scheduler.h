#pragma once

#include <cstdint>

namespace kb
{
namespace th
{

class JobSystem;
struct Job;
class Scheduler
{
public:
    Scheduler(JobSystem& js);
    virtual ~Scheduler() = default;

    // Schedule a job execution
    virtual void dispatch(Job* job) = 0;
    // Return true if the load balancing algorithm is dynamic, false otherwise
    virtual bool is_dynamic() { return false; }

protected:
    JobSystem& js_;
};

class RoundRobinScheduler : public Scheduler
{
public:
    RoundRobinScheduler(JobSystem& js);
    virtual ~RoundRobinScheduler() = default;
    virtual void dispatch(Job* job) override;

private:
    std::size_t round_robin_ = 0;
};

class MininmumLoadScheduler : public Scheduler
{
public:
    MininmumLoadScheduler(JobSystem& js);
    virtual ~MininmumLoadScheduler() = default;
    virtual void dispatch(Job* job) override;
    virtual bool is_dynamic() override { return true; }

private:
    std::size_t round_robin_ = 0;
};

} // namespace th
} // namespace kb