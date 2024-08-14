#include "kibble/memory/policy/memory_tracking_simple.h"
#include "kibble/logger/logger.h"
#include "kibble/memory/heap_area.h"

namespace kb::memory::policy
{

void SimpleMemoryTracking::init(const std::string& debug_name, const HeapArea& area)
{
    debug_name_ = debug_name;
    log_channel_ = area.get_logger_channel();
}

void SimpleMemoryTracking::report() const
{
    if (num_allocs_)
    {
        klog(log_channel_)
            .uid("MemoryTracker")
            .error("Arena: {}, Alloc-dealloc mismatch: {}", debug_name_, num_allocs_);
    }
}

} // namespace kb::memory::policy