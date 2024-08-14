#pragma once

#include "kibble/memory/util/alignment.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <stdexcept>

namespace kb
{
namespace th
{

enum class JobState : size_t
{
    Idle,      // Freshly created
    Pending,   // Scheduled
    Preempted, // Preempted externally
    Executing, // Running
    Processed  // Finished
};

/**
 * @brief Holds job dependency information, and the associated shared state.
 *
 * It allows to create an intrusive acyclic directed graph of jobs, so as to
 * schedule children jobs just in time, when their dependencies have been processed.
 * The dependencies are organized in two arrays (harnesses), one references the
 * input nodes (dependencies) and the outher the output nodes (dependents).
 *
 * @tparam T Underlying output object type
 * @tparam MAX_IN Maximum number of input nodes
 * @tparam MAX_OUT Maximum number of output nodes
 */
template <typename T, size_t MAX_IN, size_t MAX_OUT>
class ProcessNode
{
public:
    /**
     * @brief Connect this node to another
     *
     * @param to Reference to the target node
     * @param object Value associated to the output node (typically a job pointer)
     */
    inline void connect(ProcessNode& to, T object)
    {
        out_objects_[out_nodes_.count()] = object;
        out_nodes_.add(to);
        to.in_nodes_.add(*this);
        to.pending_in_.fetch_add(1, std::memory_order_release);
    }

    /// Check if there are no pending dependencies
    inline bool is_ready() const
    {
        return pending_in_.load(std::memory_order_acquire) == 0;
    }

    /// Get the number of pending dependencies
    inline size_t get_pending() const
    {
        return pending_in_.load(std::memory_order_acquire);
    }

    inline void remove_dependency()
    {
        pending_in_.fetch_sub(1, std::memory_order_release);
    }

    inline bool check_state(JobState expected)
    {
        return state_.load(std::memory_order_acquire) == expected;
    }

    inline void force_state(JobState desired)
    {
        state_.store(desired, std::memory_order_release);
    }

    inline bool exchange_state(JobState& expected, JobState desired)
    {
        return state_.compare_exchange_strong(expected, desired);
    }

    /// (Recursively) reset the shared state only, useful for jobs that are kept alive
    void reset()
    {
        // scheduled_.clear(std::memory_order_seq_cst);
        // processed_.store(false, std::memory_order_release);
        state_.store(JobState::Idle, std::memory_order_release);

        for (auto* child : out_nodes_)
        {
            child->pending_in_.fetch_add(1, std::memory_order_release);
            child->reset();
        }
    }

    // clang-format off
    /// Get iterator to the beginning of the output harness
    inline auto begin()        { return std::begin(out_objects_); }
    /// Get iterator to the end of the output harness
    inline auto end()          { return std::begin(out_objects_) + out_nodes_.count(); }
    /// Get const iterator to the beginning of the output harness
    inline auto cbegin() const { return std::cbegin(out_objects_); }
    /// Get const iterator to the end of the output harness
    inline auto cend() const   { return std::cbegin(out_objects_) + out_nodes_.count(); }
    /// Get the number of elements in the output harness
    inline size_t out_count() const { return out_nodes_.count(); }
    /// Get the number of elements in the input harness
    inline size_t in_count() const  { return in_nodes_.count(); }
    // clang-format on

private:
    /**
     * @internal
     * @brief Represent a group of connections.
     *
     * @tparam SIZE Maximum amount of connections
     */
    template <size_t SIZE>
    class Harness
    {
    public:
        /// Add a node to this harness
        inline void add(ProcessNode& node)
        {
            if (count_ + 1 == SIZE)
            {
                throw std::overflow_error("Maximum node amount reached.");
            }

            slots_[count_++] = &node;
        }

        /// Access an element by index (const version)
        inline const auto& operator[](int idx) const
        {
            if (idx >= count_)
            {
                throw std::out_of_range("Index out of bounds in harness.");
            }

            return slots_[idx];
        }

        /// Access an element by index
        inline auto& operator[](int idx)
        {
            if (idx >= count_)
            {
                throw std::out_of_range("Index out of bounds in harness.");
            }

            return slots_[idx];
        }

        // clang-format off
        /// Get iterator to the beginning
        inline auto begin()         { return std::begin(slots_); }
        /// Get iterator to the end
        inline auto end()           { return std::begin(slots_) + count_; }
        /// Get const iterator to the beginning
        inline auto cbegin() const  { return std::cbegin(slots_); }
        /// Get const iterator to the end
        inline auto cend() const    { return std::cbegin(slots_) + count_; }
        /// Get the number of elements in this harness
        inline size_t count() const { return count_; }
        // clang-format on

    private:
        std::array<ProcessNode*, SIZE> slots_;
        size_t count_ = 0;
    };

private:
    /// Dependencies
    L1_ALIGN Harness<MAX_IN> in_nodes_;
    /// Dependent nodes
    L1_ALIGN Harness<MAX_OUT> out_nodes_;
    /// Dependent objects
    L1_ALIGN std::array<T, MAX_OUT> out_objects_;
    /// Number of pending dependencies
    L1_ALIGN std::atomic<size_t> pending_in_ = 0;
    /// State of the node
    L1_ALIGN std::atomic<JobState> state_{JobState::Idle};
};

} // namespace th
} // namespace kb