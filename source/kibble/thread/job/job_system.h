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

[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ANY = std::numeric_limits<worker_affinity_t>::max();
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_MAIN = 1;
[[maybe_unused]] static constexpr worker_affinity_t WORKER_AFFINITY_ASYNC = WORKER_AFFINITY_ANY & ~WORKER_AFFINITY_MAIN;

struct JobMetadata
{
    label_t label = 0;
    worker_affinity_t worker_affinity = WORKER_AFFINITY_ANY;
    int64_t execution_time_us = 0;
};

struct Job
{
    JobMetadata meta;
    JobKernel kernel = JobKernel{}; // The function to execute
    std::vector<Job*> dependants;   // All jobs that have this one as a dependency

    // State
    std::atomic<size_t> dependency_count = {0}; // Job can be executed when this reaches 0
    std::atomic<bool> finished = {false};       // Set to true when this job has been processed

    void add_child(Job* child);
};

enum class SchedulingAlgorithm : uint8_t
{
    round_robin, // Round-robin selection of worker threads
    min_load,    // Uses monitor's execution time database for smarter assignments
};

struct JobSystemScheme
{
    size_t max_workers = 0;            // Maximum number of worker threads, if 0 => CPU_cores - 1
    size_t max_stealing_attempts = 16; // Maximum number of stealing attempts before moving to the next worker
    bool enable_work_stealing = true;  // Allow idle workers to steal jobs from their siblings
    SchedulingAlgorithm scheduling_algorithm = SchedulingAlgorithm::round_robin;
};

struct SharedState;
class Scheduler;
class Monitor;
class WorkerThread;
class JobSystem
{
public:
    JobSystem(memory::HeapArea& area, const JobSystemScheme& scheme);
    ~JobSystem();

    // Setup a job profile persistence file to load/store monitor data
    void use_persistence_file(const fs::path& filepath);
    // Wait for all jobs to finish, join worker threads and destroy system storage
    void shutdown();
    // Create a new job
    Job* create_job(JobKernel&& kernel, const JobMetadata& meta = JobMetadata{});
    // Return job to the pool
    void release_job(Job* job);
    // Schedule job execution
    void schedule(Job* job);
    // Non-blockingly check if any worker threads are busy
    bool is_busy() const;
    // Non-blockingly check if a job is processed
    bool is_work_done(Job* job) const;
    // Wait for input condition to become false, synchronous work may be executed in the meantime
    void wait_untill(std::function<bool()> condition);
    // Hold execution on this thread untill all jobs are processed or predicate returns false
    void wait(std::function<bool()> condition = []() { return true; });
    // Hold execution on this thread untill a given job has been processed or predicate returns false
    void wait_for(
        Job* job, std::function<bool()> condition = []() { return true; });

    inline size_t get_threads_count() const { return threads_count_; }
    inline size_t get_CPU_cores_count() const { return CPU_cores_count_; }
    inline const auto& get_scheme() const { return scheme_; }
    inline auto& get_workers() { return workers_; }
    inline const auto& get_workers() const { return workers_; }
    inline auto& get_worker(size_t idx) { return *workers_[idx]; }
    inline const auto& get_worker(size_t idx) const { return *workers_[idx]; }
    inline auto& get_monitor() { return *monitor_; }
    inline const auto& get_monitor() const { return *monitor_; }
    inline auto& get_scheduler() { return *scheduler_; }
    inline const auto& get_scheduler() const { return *scheduler_; }
    inline auto& get_shared_state() { return *ss_; }
    inline const auto& get_shared_state() const { return *ss_; }

private:
    size_t CPU_cores_count_ = 0;
    size_t threads_count_ = 0;
    JobSystemScheme scheme_;
    std::vector<WorkerThread*> workers_;
    Scheduler* scheduler_;
    Monitor* monitor_;
    std::shared_ptr<SharedState> ss_;
    fs::path persistence_file_;
    bool use_persistence_file_ = false;
};

} // namespace th
} // namespace kb