#pragma once

#include <cstdint>
#include <array>
#include <atomic>
#include <stdexcept>

namespace kb
{
namespace th
{

template <size_t MAX_IN, size_t MAX_OUT>
class ProcessNode
{
public:
    inline void connect(ProcessNode &to)
    {
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

    inline bool is_processed() const
    {
        return processed_.load();
    }

    void mark_processed()
    {
        processed_.store(true);
        for (auto *child : out_nodes_)
            child->pending_in_.fetch_sub(1);
    }

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
        inline auto begin()        { return std::begin(slots_); }
        inline auto end()          { return std::begin(slots_) + count_; }
        inline auto cbegin() const { return std::cbegin(slots_); }
        inline auto cend() const   { return std::cbegin(slots_) + count_; }
        // clang-format on

    private:
        std::array<ProcessNode *, SIZE> slots_;
        size_t count_ = 0;
    };

private:
    std::atomic<size_t> pending_in_ = 0;
    std::atomic<bool> processed_ = false;
    Harness<MAX_IN> in_nodes_;
    Harness<MAX_OUT> out_nodes_;
};

} // namespace th
} // namespace kb