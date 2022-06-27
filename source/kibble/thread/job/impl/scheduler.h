#pragma once

#include "thread/alignment.h"
#include "thread/job/config.h"
#include <array>
#include <cstdint>

namespace kb
{
namespace th
{

class JobSystem;
struct Job;

/**
 * @brief Base scheduler class.
 * A Scheduler's responsibility is to dispatch jobs to workers. The way they do it is implementation-defined. The
 * concept of load balancing is of central importance when designing a scheduler. Several strategies can be used to
 * balance thread load. This polymorphic design allows to dynamically choose such a strategy among multiple
 * implementations.
 * Schedulers will be called by workers when they finished a parent task and need to schedule children tasks. So
 * they need to be thread-safe. It is the responsibility of the implementation to avoid data races and deadlocks.
 *
 */
class Scheduler
{
public:
    /**
     * @brief Construct a new Scheduler.
     *
     * @param js job system instance
     */
    Scheduler(JobSystem &js);

    virtual ~Scheduler() = default;

    /**
     * @brief Schedule a job execution.
     *
     * @param job job pointer
     */
    virtual void dispatch(Job *job) = 0;

    /**
     * @brief Check if the balancing algorithm is dynamic.
     * A dynamic load balancing algorithm uses job statistics to make informed dispatch decisions. If the job system
     * detects a dynamic load balancing algorithm, it will supply the monitor with a flow of job reports.
     *
     * @return true if the load balancing algorithm is dynamic
     * @return false otherwise
     */
    virtual bool is_dynamic()
    {
        return false;
    }

protected:
    PAGE_ALIGN JobSystem &js_;
};

/**
 * @brief This Scheduler dispatches the next job to the next worker in the line.
 * This simple static load balancing strategy ensures that a given worker is never given a job twice in a row, which
 * gives it some time to process its job queue before a new job is pushed.
 *
 * @note This scheduler is thread-safe.
 *
 */
class RoundRobinScheduler : public Scheduler
{
public:
    /**
     * @brief Construct a new RoundRobinScheduler.
     *
     * @param js job system instance
     */
    RoundRobinScheduler(JobSystem &js);

    /**
     * @brief Hand this job to the next worker.
     * If the job declares a non-default worker affinity in its metadata, the round robin will cycle until the
     * appropriate worker can handle it.
     *
     * @param job job instance
     */
    void dispatch(Job *job) override final;

private:
    // Each thread has its own round robin state (64b to avoid false sharing)
    PAGE_ALIGN std::array<std::size_t, k_max_threads> round_robin_;
};

/**
 * @brief This Scheduler dispatches the next job to the worker with the less load.
 * This dynamic load balancing strategy benefits from Monitor input to make informed dispatch decisions.
 *
 * @note This scheduler is thread safe. It does not seem to perform as well as advertised when a task graph is used.
 * The RoundRobinScheduler seem more efficient in this case. This may be due to worker affinity being adversarial in
 * this context, by inducing loads of job resubmissions.
 *
 */
class MinimumLoadScheduler : public Scheduler
{
public:
    /**
     * @brief Construct a new MinimumLoadScheduler.
     *
     * @param js job system instance
     */
    MinimumLoadScheduler(JobSystem &js);

    /**
     * @brief Hand this job to the optimally selected worker.
     * A list of viable worker candidates is constructed based on worker affinity in the job's metadata, then the job is
     * assigned to the worker with minimal load among this list.
     * If the job is unlabelled, no job size information can be queried from the monitor, then this function will
     * fallback to a round robin scheme to select the next worker.
     *
     * @param job job instance
     */
    void dispatch(Job *job) override final;

    /**
     * @brief This is a dynamic load balancer, return true.
     *
     * @return true
     */
    bool is_dynamic() override final
    {
        return true;
    }

private:
    // Each thread has its own round robin state (64b to avoid false sharing)
    PAGE_ALIGN std::array<std::size_t, k_max_threads> round_robin_;
};

} // namespace th
} // namespace kb