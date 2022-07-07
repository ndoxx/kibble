#pragma once

#include "thread/alignment.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <stdexcept>

namespace kb
{
namespace th
{

template <typename T, size_t MAX_IN, size_t MAX_OUT>
class ProcessNode
{
public:
    inline void connect(ProcessNode &to, T object)
    {
        out_objects_[out_nodes_.count()] = object;
        out_nodes_.add(to);
        to.in_nodes_.add(*this);
        to.pending_in_.fetch_add(1);
    }

    inline const auto &get_in_nodes() const
    {
        return in_nodes_;
    }

    inline const auto &get_out_nodes() const
    {
        return out_nodes_;
    }

    inline bool is_ready() const
    {
        return pending_in_.load() == 0;
    }

    inline size_t get_pending() const
    {
        return pending_in_.load();
    }

    inline bool is_processed() const
    {
        return processed_.load();
    }

    inline auto &scheduled()
    {
        return scheduled_;
    }

    void mark_processed()
    {
        for (auto *child : out_nodes_)
            child->pending_in_.fetch_sub(1);

        processed_.store(true);
    }

    bool mark_scheduled()
    {
        return !scheduled_.test_and_set();
    }

    void reset()
    {
        scheduled_.clear();
        processed_.store(false);

        for (auto *child : out_nodes_)
        {
            child->pending_in_.fetch_add(1);
            child->reset();
        }
    }

    // clang-format off
    inline auto begin()        { return std::begin(out_objects_); }
    inline auto end()          { return std::begin(out_objects_) + out_nodes_.count(); }
    inline auto cbegin() const { return std::cbegin(out_objects_); }
    inline auto cend() const   { return std::cbegin(out_objects_) + out_nodes_.count(); }
    // clang-format on

private:
    template <size_t SIZE>
    class Harness
    {
    public:
        inline void add(ProcessNode &node)
        {
            if (count_ + 1 == SIZE)
                throw std::overflow_error("Maximum node amount reached.");

            slots_[count_++] = &node;
        }

        inline const auto &operator[](int idx) const
        {
            if (idx >= count_)
                throw std::out_of_range("Index out of bounds in harness.");

            return slots_[idx];
        }

        inline auto &operator[](int idx)
        {
            if (idx >= count_)
                throw std::out_of_range("Index out of bounds in harness.");

            return slots_[idx];
        }

        // clang-format off
        inline auto begin()         { return std::begin(slots_); }
        inline auto end()           { return std::begin(slots_) + count_; }
        inline auto cbegin() const  { return std::cbegin(slots_); }
        inline auto cend() const    { return std::cbegin(slots_) + count_; }
        inline size_t count() const { return count_; }
        // clang-format on

    private:
        std::array<ProcessNode *, SIZE> slots_;
        size_t count_ = 0;
    };

private:
    /// Dependencies
    PAGE_ALIGN Harness<MAX_IN> in_nodes_;
    /// Dependent nodes
    PAGE_ALIGN Harness<MAX_OUT> out_nodes_;
    /// Dependent objects
    PAGE_ALIGN std::array<T, MAX_OUT> out_objects_;
    /// Number of pending dependencies
    PAGE_ALIGN std::atomic<size_t> pending_in_ = 0;
    /// Set to true as soon as node has been processed
    PAGE_ALIGN std::atomic<bool> processed_ = false;
    /// To avoid multiple scheduling of children
    PAGE_ALIGN std::atomic_flag scheduled_ = ATOMIC_FLAG_INIT;
};

} // namespace th
} // namespace kb