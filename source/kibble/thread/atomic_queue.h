#pragma once

/*
 * -------------- DO NOT USE --------------
 * Adapted from https://manu343726.github.io/2017-03-13-lock-free-job-stealing-task-system-with-modern-c/
 * -> Mimicks max0x7ba's atomic_queue API (https://github.com/max0x7ba/atomic_queue)
 * -> Designed to implement (FIFO) work stealing later on
 * -> Highly experimental (meaning: bug-riddled junk)
 */

#include "thread/intrin.h"
#include <array>
#include <atomic>

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
    bool was_empty() const noexcept;

    void push(T element) noexcept;
    T pop() noexcept;

    inline size_t capacity() const { return N; }
    inline size_t was_size()
    {
        return size_t(std::max(int(bottom_.load(std::memory_order_relaxed)) - int(top_.load(std::memory_order_relaxed)), 0));
    }
    inline bool was_full() { return was_size() >= capacity(); }
    inline bool was_empty() { return !was_size(); }

private:
	static constexpr T NIL = T{};

    alignas(k_cache_line_size) std::atomic<size_t> top_;
    alignas(k_cache_line_size) std::atomic<size_t> bottom_;
    alignas(k_cache_line_size) std::array<std::atomic<T>, N> elements_;
};

template <typename T, size_t N> AtomicQueue<T, N>::AtomicQueue()
{
    K_ASSERT(std::atomic<T>{NIL}.is_lock_free(), "AtomicQueue only works with atomic elements.");
    top_.store(0, std::memory_order_release);
    bottom_.store(0, std::memory_order_release);
    std::fill(elements_.begin(), elements_.end(), NIL);
}

template <typename T, size_t N> bool AtomicQueue<T, N>::try_push(T element) noexcept
{
    size_t bottom = bottom_.load(std::memory_order_acquire);

    if(bottom < capacity())
    {
        elements_[bottom].store(element, std::memory_order_release);
        // bottom_.store(bottom + 1, std::memory_order_release);
        bottom_.fetch_add(1, std::memory_order_release);
        return true;
    }
    else
        return false;
}

template <typename T, size_t N> bool AtomicQueue<T, N>::try_pop(T& element) noexcept
{
    size_t bottom = bottom_.load(std::memory_order_relaxed);
    bottom = std::max(0ul, bottom - 1ul);
    bottom_.store(bottom, std::memory_order_seq_cst);
    size_t top = top_.load(std::memory_order_seq_cst);

    if(top <= bottom)
    {
    	T elt = elements_[bottom].load(std::memory_order_relaxed);

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
            if(!top_.compare_exchange_strong(expected_top, desired_top, std::memory_order_relaxed))
            {
                elt = NIL;
                success = false;
            }

            bottom_.store(top + 1, std::memory_order_relaxed);
            element = elt;
            return success;
        }
    }
    else
    {
        // Queue is empty
        bottom_.store(top, std::memory_order_relaxed);
        element = nullptr;
        return false;
    }
}

template <typename T, size_t N> bool AtomicQueue<T, N>::try_steal(T& element) noexcept
{
	// TODO: This never wraps, capacity shrinks each time an element is stolen...
    size_t top = top_.load(std::memory_order_relaxed);
    size_t bottom = bottom_.load(std::memory_order_seq_cst);

    if(top < bottom)
    {
        // Queue is non-empty
    	// SHOULD BE an atomic array:
    	T elt = elements_[bottom].load(std::memory_order_relaxed);

        size_t expected_top = top;
        size_t desired_top = top + 1;
        // If this CAS fails, it means steal lost the race against a concurrent steal/pop, abort
        if(!top_.compare_exchange_weak(expected_top, desired_top, std::memory_order_seq_cst))
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

template <typename T, size_t N> void AtomicQueue<T, N>::push(T element) noexcept
{
    while(!try_push(element))
        intrin::spin__();
}

template <typename T, size_t N> T AtomicQueue<T, N>::pop() noexcept
{
    T element = NIL;
    while(!try_pop(element))
        intrin::spin__();
    return element;
}

} // namespace kb