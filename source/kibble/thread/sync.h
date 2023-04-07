#pragma once

#include <atomic>

namespace kb
{

/**
 * @brief Spinlock synchronization primitive.
 * This implementation uses an atomic flag and a busy wait.
 *
 */
struct Spinlock
{
    std::atomic<bool> lock_ = {false};

    /**
     * @brief lock the primitive.
     * A thread that attempts to acquire it will wait in a loop.
     *
     */
    void lock();

    /**
     * @brief unlock the primitive.
     *
     */
    void unlock();
};

} // namespace kb