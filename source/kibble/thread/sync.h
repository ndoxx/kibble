#pragma once

#include <atomic>

namespace kb
{

struct SpinLock
{
    std::atomic<bool> lock_ = {false};

    void lock();
    void unlock();
};

} // namespace kb