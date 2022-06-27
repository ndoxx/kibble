#pragma once

#include <cstdint>

namespace kb
{
namespace th
{

/// Maximum allowable number of worker threads
[[maybe_unused]] static constexpr std::size_t k_max_threads = CFG_JS_MAX_THREADS;
/// Maximum number of jobs per worker thread queue
[[maybe_unused]] static constexpr std::size_t k_max_jobs = CFG_JS_MAX_JOBS_PER_WORKER;
/// Maximum number of stats packets in the monitor queue
[[maybe_unused]] static constexpr std::size_t k_stats_queue_capacity = CFG_JS_STATS_QUEUE_CAPACITY;
// Maximum number of dependent jobs for each job
[[maybe_unused]] static constexpr std::size_t k_max_child_jobs = CFG_JS_MAX_CHILD_JOBS;
// Maximum number of job dependencies
[[maybe_unused]] static constexpr std::size_t k_max_parent_jobs = CFG_JS_MAX_PARENT_JOBS;

} // namespace th
} // namespace kb