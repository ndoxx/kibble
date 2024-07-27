#include "thread/sync.h"
#include "thread/impl/intrin.h"

namespace kb
{

void Spinlock::lock()
{
    for (;;)
    {
        if (!lock_.exchange(true, std::memory_order_acquire))
        {
            break;
        }
        while (lock_.load(std::memory_order_relaxed))
            ;
    }
}

void Spinlock::unlock()
{
    lock_.store(false, std::memory_order_release);
}

} // namespace kb