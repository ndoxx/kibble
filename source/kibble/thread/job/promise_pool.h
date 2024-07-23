#pragma once

#include "../../util/sparse_set.h"

#include <mutex>

namespace kb::th
{

template <typename T, size_t PoolSize = 1024>
class PromisePool
{
private:
    alignas(T) char memory_[PoolSize * sizeof(T)];
    SparsePool<uint32_t, PoolSize> handles_;
    std::mutex mutex_;

public:
    T* allocate()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handles_.size() >= PoolSize)
        {
            throw std::bad_alloc();
        }
        uint32_t index = handles_.acquire();
        return reinterpret_cast<T*>(&memory_[index * sizeof(T)]);
    }

    void deallocate(T* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t index = uint32_t(reinterpret_cast<char*>(ptr) - memory_) / sizeof(T);
        handles_.release(index);
    }
};

template <typename T>
class PromiseAllocator
{
private:
    static PromisePool<T>& pool()
    {
        static PromisePool<T> pool;
        return pool;
    }

public:
    using value_type = T;

    PromiseAllocator() = default;
    template <typename U>
    PromiseAllocator(const PromiseAllocator<U>&)
    {
    }

    T* allocate(std::size_t n)
    {
        if (n != 1)
        {
            throw std::bad_alloc();
        }
        return pool().allocate();
    }

    void deallocate(T* p, std::size_t n)
    {
        if (n != 1)
        {
            return;
        }
        pool().deallocate(p);
    }
};

// Standard-compliant allocators must be comparable
template <typename T, typename U>
bool operator==(const PromiseAllocator<T>&, const PromiseAllocator<U>&)
{
    return true;
}

} // namespace kb::th