#pragma once

#include <cstdint>

namespace kb
{
namespace th
{

/// Maximum allowable number of worker threads
[[maybe_unused]] static constexpr std::size_t k_max_threads = 8;
/// Maximum number of jobs per worker thread queue
[[maybe_unused]] static constexpr std::size_t k_max_jobs = 1024;
/// Maximum number of stats packets in the monitor queue
[[maybe_unused]] static constexpr std::size_t k_stats_queue_capacity = 128;
// Maximum number of dependent jobs for each job
[[maybe_unused]] static constexpr std::size_t k_max_child_jobs = 8;
// Maximum number of job dependencies
[[maybe_unused]] static constexpr std::size_t k_max_parent_jobs = 4;

} // namespace th
} // namespace kb