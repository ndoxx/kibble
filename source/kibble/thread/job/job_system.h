#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace fs = std::filesystem;

namespace kb
{
namespace memory
{
class HeapArea;
} // namespace memory

namespace th
{

using JobKernel = std::function<void(void)>;
using worker_affinity_t = uint32_t;
using label_t = uint64_t;
using tid_t = uint32_t;

/// A job with this affinity can be executed on any worker
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ANY = std::numeric_limits<worker_affinity_t>::max();
/// A job with this affinity should be executed on the main thread
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_MAIN = 1;
/// A job with this affinity should be executed on any background thread
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ASYNC = WORKER_AFFINITY_ANY & ~WORKER_AFFINITY_MAIN;

/**
 * @brief Metadata associated to a job.
 *
 */
struct JobMetadata
{
    /// Uniquely identifies a job
    label_t label = 0;
    /// Workers this job can be pushed to
    worker_affinity_t worker_affinity = WORKER_AFFINITY_ANY;
    /// The time in µs it took to complete the job
    int64_t execution_time_us = 0;
};

/**
 * @brief Represents some amount of work to execute.
 *
 */
struct Job
{
    /// Job metadata
    JobMetadata meta;
    /// The function to execute
    JobKernel kernel = JobKernel{};
    /// All jobs that have this one as a dependency
    std::vector<Job *> children;

    // State
    /// Set to true when this job has been processed
    std::atomic<bool> finished = {false};

    /**
     * @brief Add a job that can only be executed once this job was processed.
     *
     * @warning Children jobs must never be scheduled by hand. The worker in charge of this job will schedule the
     * children by himself once this job has been processed.
     *
     * @param child the child to add.
     */
    inline void add_child(Job *child)
    {
        children.push_back(child);
    }
};

/**
 * @brief Enumerates the job scheduling algorithms.
 *
 */
enum class SchedulingAlgorithm : uint8_t
{
    /// Round-robin selection of worker threads
    round_robin,
    /// Uses monitor's execution time database for smarter assignments
    min_load,
};

/**
 * @brief Job system configuration structure.
 *
 */
struct JobSystemScheme
{
    /// Maximum number of worker threads, if 0 => CPU_cores - 1
    size_t max_workers = 0;
    /// Maximum number of stealing attempts before moving to the next worker
    size_t max_stealing_attempts = 16;
    /// Allow idle workers to steal jobs from their siblings
    bool enable_work_stealing = true;
    /// Job scheduling policy
    SchedulingAlgorithm scheduling_algorithm = SchedulingAlgorithm::round_robin;
};

struct SharedState;
class Scheduler;
class Monitor;
class WorkerThread;

/**
 * @brief Assign work to multiple worker threads.
 * The job system implementation is split into multiple single-responsibility components. All these components are
 * coordinated by this master system.
 *
 * @see Monitor Scheduler WorkerThread
 *
 */
class JobSystem
{
public:
    /**
     * @brief Construct a new Job System.
     * The job system uses a memory pool for fast job allocation, that's why a heap area is needed.
     * On creation, all subsystems are initialized. Then a certain number of worker threads are spawned, no more than
     * the maximum number prescribed by the scheme structure, and no more than the amount of CPU cores minus one, on the
     * present architecture.
     *
     * @param area heap area for the job pool
     * @param scheme configuration structure
     */
    JobSystem(memory::HeapArea &area, const JobSystemScheme &scheme);

    /**
     * @brief Calls shutdown() before destruction.
     *
     */
    ~JobSystem();

    /**
     * @brief Setup a job profile persistence file to load/store monitor data.
     * This file will gather execution time statistics for labelled jobs, which will enable the minimum load scheduler
     * to use past execution metada during this run.
     *
     * @param filepath path to the persistence file
     */
    void use_persistence_file(const fs::path &filepath);

    /**
     * @brief Wait for all jobs to finish, join worker threads and destroy system storage
     *
     */
    void shutdown();

    /**
     * @brief Create a new job.
     * @note The returned job belongs to an internal job pool and should never be deleted manually. Once caller code is
     * done with the job, the function release_job() should be called to return the job to the pool.
     *
     * @param kernel function containing code to execute.
     * @param meta job metadata. Can be used to give a unique label to this job and setup worker affinity.
     * @return a new job from the pool
     */
    Job *create_job(JobKernel &&kernel, const JobMetadata &meta = JobMetadata{});

    /**
     * @brief Return job to the pool.
     *
     * @param job the job to release
     */
    void release_job(Job *job);

    /**
     * @brief Schedule job execution.
     * The number of pending jobs will be increased, the job dispatched and all worker threads will be awakened.
     *
     * @param job the job to submit
     * @param caller_thread the id of the thread calling this method (default: main thread)
     */
    void schedule(Job *job, tid_t caller_thread = 0);

    /**
     * @brief Non-blockingly check if any worker threads are busy.
     *
     * @return true is at least one worker thread is currently unidle
     * @return false otherwise
     */
    bool is_busy() const;

    /**
     * @brief Non-blockingly check if a job is processed.
     *
     * @param job the job
     * @return true it the job was processed
     * @return false otherwise
     */
    bool is_work_done(Job *job) const;

    /**
     * @brief Wait for an input condition to become false, synchronous work may be executed in the meantime.
     *
     * @param condition predicate that will keep the function busy till it evaluates to false
     */
    void wait_until(std::function<bool()> condition);

    /**
     * @brief Hold execution on this thread until all jobs are processed or the input predicate returns false.
     * Allows to wait for all jobs to be processed. This marks a sync point in the caller code.
     *
     * @param condition predicate that will keep the function busy till it evaluates to false
     */
    void wait(std::function<bool()> condition = []() { return true; });

    /**
     * @brief Hold execution on the main thread until a given job has been processed or the predicate returns false.
     *
     * @param job the job to wait for
     * @param condition predicate that will keep the function busy till it evaluates to false
     */
    void wait_for(
        Job *job, std::function<bool()> condition = []() { return true; });

    /**
     * @brief Get a list of all workers compatible with the given affinity requirement.
     *
     * @param affinity
     * @return std::vector<WorkerThread*>
     */
    std::vector<WorkerThread *> get_compatible_workers(worker_affinity_t affinity);

    /**
     * @brief Get a list of ids of all workers compatible with the given affinity requirement.
     *
     * @param affinity
     * @return std::vector<WorkerThread*>
     */
    std::vector<tid_t> get_compatible_worker_ids(worker_affinity_t affinity);

    /// Get the list of workers (non-const).
    inline auto &get_workers()
    {
        return workers_;
    }

    /// Get the list of workers (const).
    inline const auto &get_workers() const
    {
        return workers_;
    }

    /// Get the number of threads
    inline size_t get_threads_count() const
    {
        return threads_count_;
    }

    /// Get the number of CPU cores on this machine.
    inline size_t get_CPU_cores_count() const
    {
        return CPU_cores_count_;
    }

    /// Get the configuration object passed to this system at construction.
    inline const auto &get_scheme() const
    {
        return scheme_;
    }

    /// Get the worker at input index (non-const).
    inline auto &get_worker(size_t idx)
    {
        return *workers_[idx];
    }

    /// Get the worker at input index (const).
    inline const auto &get_worker(size_t idx) const
    {
        return *workers_[idx];
    }

    /// Get the monitor (non-const).
    inline auto &get_monitor()
    {
        return *monitor_;
    }

    /// Get the monitor (const).
    inline const auto &get_monitor() const
    {
        return *monitor_;
    }

    /// Get the scheduler (non-const).
    inline auto &get_scheduler()
    {
        return *scheduler_;
    }

    /// Get the scheduler (const).
    inline const auto &get_scheduler() const
    {
        return *scheduler_;
    }

    /// Get the shared state (non-const).
    inline auto &get_shared_state()
    {
        return *ss_;
    }

    /// Get the shared state (const).
    inline const auto &get_shared_state() const
    {
        return *ss_;
    }

private:
    size_t CPU_cores_count_ = 0;
    size_t threads_count_ = 0;
    JobSystemScheme scheme_;
    std::vector<WorkerThread *> workers_;
    Scheduler *scheduler_;
    Monitor *monitor_;
    std::shared_ptr<SharedState> ss_;
    fs::path persistence_file_;
    bool use_persistence_file_ = false;
};

} // namespace th
} // namespace kb