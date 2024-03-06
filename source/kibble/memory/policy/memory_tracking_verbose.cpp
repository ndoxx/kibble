#include "memory/policy/memory_tracking_verbose.h"
#include "logger2/logger.h"
#include "memory/heap_area.h"
#include "memory/util/arithmetic.h"

namespace kb::memory::policy
{

void VerboseMemoryTracking::init(const std::string& debug_name, const HeapArea& area)
{
    debug_name_ = debug_name;
    log_channel_ = area.get_logger_channel();
}

void VerboseMemoryTracking::on_allocation(uint8_t* begin, std::size_t decorated_size, std::size_t alignment,
                                          const char* file, int line)
{
    ++num_allocs_;
    allocations_.insert({reinterpret_cast<size_t>(begin), {begin, decorated_size, alignment, file, line}});

    if (log_channel_)
    {
        klog(log_channel_)
            .uid("Arena")
            .verbose(R"({} -- Allocation:
Decorated size: {}
Begin ptr:      {:#x}
Alignment:      {}B,
Location:       {}:{})",
                     debug_name_, kb::memory::utils::human_size(decorated_size), reinterpret_cast<uint64_t>(begin),
                     alignment, file, line);
    }
}

void VerboseMemoryTracking::on_deallocation(uint8_t* begin, std::size_t decorated_size, const char* file, int line)
{
    --num_allocs_;
    if (auto it = allocations_.find(reinterpret_cast<size_t>(begin)); it != allocations_.end())
    {
        allocations_.erase(it);
    }

    if (log_channel_)
    {
        klog(log_channel_)
            .uid("Arena")
            .verbose(R"({} -- Deallocation:
Decorated size: {}
Begin ptr:      {:#x}
Location:       {}:{})",
                     debug_name_, kb::memory::utils::human_size(decorated_size), reinterpret_cast<uint64_t>(begin),
                     file, line);
    }
}

void VerboseMemoryTracking::report() const
{
    if (num_allocs_)
    {
        klog(log_channel_)
            .uid("MemoryTracker")
            .error("Arena: {}, Alloc-dealloc mismatch: {}", debug_name_, num_allocs_);

        for (auto&& [key, info] : allocations_)
        {
            klog(log_channel_)
                .uid("MemoryTracker")
                .info(R"(Unresolved:
begin: {}
decorated size: {}
alignment: {}
location: {}:{})",
                      reinterpret_cast<void*>(info.begin), info.decorated_size, info.alignment, info.file, info.line);
        }
    }
}

} // namespace kb::memory::policy