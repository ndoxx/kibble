#pragma once

namespace kb::memory::policy
{

/**
 *
 * @brief MemoryArena thread-guard policy that can hold mutex-like primitives.
 * It will work as-is with std::mutex, but may need to be specialized for other synchronization primitives that do not
 * share the same API.
 *
 * @tparam SynchronizationPrimitiveT type of the synchronization primitive
 */
template <typename SynchronizationPrimitiveT>
class MultiThread
{
public:
    /**
     * @brief Lock the primitive on entering the thread guard
     *
     */
    inline void enter()
    {
        primitive_.lock();
    }

    /**
     * @brief Unlock the primitive on leaving the thread guard
     *
     */
    inline void leave()
    {
        primitive_.unlock();
    }

private:
    SynchronizationPrimitiveT primitive_;
};

} // namespace kb::memory::policy