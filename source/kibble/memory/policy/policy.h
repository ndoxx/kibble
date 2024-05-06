#pragma once

#include <concepts>
#include <cstdint>
#include <string>

namespace kb::log
{
class Channel;
}

namespace kb::memory
{
class HeapArea;
}

namespace kb::memory::policy
{

class NullPolicy
{
};

/// @brief Null MemoryArena thread-guard policy prescribed for single-threaded use cases.
using SingleThread = NullPolicy;

/// @brief Null bounds checking sanitization policy used by MemoryArena.
using NoBoundsChecking = NullPolicy;

/// @brief Null memory tagging policy.
using NoMemoryTagging = NullPolicy;

/// @brief Null memory tracking policy.
using NoMemoryTracking = NullPolicy;

template <typename T>
concept is_active_threading_policy = requires(T& policy) {
    // clang-format off
    { policy.enter() } -> std::same_as<void>;
    { policy.leave() } -> std::same_as<void>;
    // clang-format on
};

template <typename T>
concept is_active_bounds_checking_policy = requires(const T& policy) {
    // clang-format off
    { policy.put_sentinel_front(nullptr) }   -> std::same_as<void>;
    { policy.put_sentinel_back(nullptr) }    -> std::same_as<void>;
    { policy.check_sentinel_front(nullptr) } -> std::same_as<void>;
    { policy.check_sentinel_back(nullptr) }  -> std::same_as<void>;
    // clang-format on
};

template <typename T>
concept is_active_memory_tagging_policy = requires(const T& policy) {
    // clang-format off
    { policy.tag_allocation(nullptr, 0) }   -> std::same_as<void>;
    { policy.tag_deallocation(nullptr, 0) } -> std::same_as<void>;
    // clang-format on
};

template <typename T>
concept is_active_memory_tracking_policy = requires(T& policy, const std::string& debug_name, const HeapArea& area) {
    // clang-format off
    { policy.init(debug_name, area) }                   -> std::same_as<void>;
    { policy.on_allocation(nullptr, 0, 0, nullptr, 0) } -> std::same_as<void>;
    { policy.on_deallocation(nullptr, 0, nullptr, 0) }  -> std::same_as<void>;
    { policy.get_allocation_count() }                   -> std::convertible_to<int32_t>;
    { policy.report() }                                 -> std::same_as<void>;
    // clang-format on
};

template <typename AllocatorT>
concept has_reallocate = requires(AllocatorT& allocator, void* ptr, std::size_t old_size, std::size_t new_size) {
    // clang-format off
    { allocator.reallocate(ptr, old_size, new_size) } -> std::same_as<void*>;
    // clang-format on
};

template <typename T>
struct BoundsCheckerSentinelSize
{
    static constexpr std::size_t FRONT = 0;
    static constexpr std::size_t BACK = 0;
};

} // namespace kb::memory::policy