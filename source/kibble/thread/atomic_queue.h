#pragma once

/*
 * Adapted from https://manu343726.github.io/2017-03-13-lock-free-job-stealing-task-system-with-modern-c/
 */

#include <array>
#include <atomic>
#include "thread/intrin.h"

namespace kb
{

static constexpr size_t k_cache_line_size = 64;

template <typename T, size_t N> class AtomicQueue
{
public:
    AtomicQueue();

    bool try_push(T element) noexcept;
    bool try_pop(T& element) noexcept;
    bool try_steal(T& element) noexcept;
    bool empty() const noexcept;

    void push(T element) noexcept;
    T pop() noexcept;

    inline size_t size() const { return N; }

private:
    alignas(k_cache_line_size) std::atomic<size_t> top_;
    alignas(k_cache_line_size) std::atomic<size_t> bottom_;
    alignas(k_cache_line_size) std::array<T, N> elements_;
};

template <typename T, size_t N> AtomicQueue<T, N>::AtomicQueue()
{
    top_.store(0, std::memory_order_release);
    bottom_.store(0, std::memory_order_release);
    std::fill(elements_.begin(), elements_.end(), T(0));
}

template <typename T, size_t N> bool AtomicQueue<T, N>::try_push(T element) noexcept
{
    size_t bottom = bottom_.load(std::memory_order_acquire);

    if(bottom < size())
    {
        elements_[bottom] = element;
        // bottom_.store(bottom + 1, std::memory_order_release);
        bottom_.fetch_add(1, std::memory_order_release);
        return true;
    }
    else
        return false;
}

template <typename T, size_t N> bool AtomicQueue<T, N>::try_pop(T& element) noexcept
{
    size_t bottom = bottom_.load(std::memory_order_acquire);
    bottom = std::max(0ul, bottom - 1ul);
    bottom_.store(bottom, std::memory_order_release);
    size_t top = top_.load(std::memory_order_acquire);

    if(top <= bottom)
    {
        T* elt = elements_[bottom];

        if(top != bottom)
        {
            // More than one job left in the queue, no problem
            element = elt;
            return true;
        }
        else
        {
            size_t expected_top = top;
            size_t desired_top = top + 1;
            bool success = true;
            // If this CAS fails, it means pop lost the race against a concurrent steal, abort
            if(!top_.compare_exchange_weak(expected_top, desired_top, std::memory_order_acq_rel))
            {
                elt = T(0);
                success = false;
            }

            bottom_.store(top + 1, std::memory_order_release);
            element = elt;
            return success;
        }
    }
    else
    {
        // Queue is empty
        bottom_.store(top, std::memory_order_release);
        element = nullptr;
        return false;
    }
}

template <typename T, size_t N> bool AtomicQueue<T, N>::try_steal(T& element) noexcept
{
    size_t top = top_.load(std::memory_order_acquire);
    size_t bottom = bottom_.load(std::memory_order_acquire);

    if(top < bottom)
    {
        // Queue is non-empty
        T* elt = elements_[top];

        size_t expected_top = top;
        size_t desired_top = top + 1;
        // If this CAS fails, it means steal lost the race against a concurrent steal/pop, abort
        if(!top_.compare_exchange_weak(expected_top, desired_top, std::memory_order_acq_rel))
        {
            return false;
        }

        element = elt;
        return true;
    }
    else
    {
        // Queue is empty
        return false;
    }
}

template <typename T, size_t N> bool AtomicQueue<T, N>::empty() const noexcept
{
    size_t top = top_.load(std::memory_order_acquire);
    size_t bottom = bottom_.load(std::memory_order_acquire);
    return bottom > top;
}

template <typename T, size_t N> void AtomicQueue<T, N>::push(T element) noexcept
{
    while(!try_push(element))
        intrin::spin__();
}

template <typename T, size_t N> T AtomicQueue<T, N>::pop() noexcept
{
    T element;
    while(!try_pop(element))
        intrin::spin__();
    return element;
}

} // namespace kb