#include "kibble/logger/formatters/vscode_terminal_formatter.h"
#include "kibble/logger/logger.h"
#include "kibble/logger/policies/stack_trace_policy.h"
#include "kibble/logger/policies/uid_filter.h"
#include "kibble/logger/sinks/console_sink.h"
#include "kibble/logger/sinks/file_sink.h"
#include "kibble/math/color_table.h"
#include "kibble/memory/allocator/pool_allocator.h"
#include "kibble/memory/arena.h"
#include "kibble/memory/heap_area.h"
#include "kibble/memory/policy/bounds_checking_simple.h"
#include "kibble/memory/policy/memory_tracking_verbose.h"
#include "kibble/memory/util/literals.h"

using namespace kb::memory::literals;
using namespace kb::log;
using namespace kb;

/*
    The PoolAllocator allocation policy makes the MemoryArena a memory pool.
    Here we use a VerboseMemoryTracking policy to fully track allocations.
    In a retail build, we could use the NoMemoryTracking policy instead, which
    completly suppresses memory tracking and the associated overhead.
*/

#ifdef K_DEBUG
using MemoryPool =
    kb::memory::MemoryArena<kb::memory::PoolAllocator, kb::memory::policy::SingleThread,
                            kb::memory::policy::SimpleBoundsChecking, kb::memory::policy::NoMemoryTagging,
                            kb::memory::policy::VerboseMemoryTracking>;
#else
using MemoryPool = kb::memory::MemoryArena<kb::memory::PoolAllocator, kb::memory::policy::SingleThread,
                                           kb::memory::policy::NoBoundsChecking, kb::memory::policy::NoMemoryTagging,
                                           kb::memory::policy::VerboseMemoryTracking>;
#endif

struct Data
{
    size_t x;
    float y;
};

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(std::make_shared<VSCodeTerminalFormatter>());

    Channel chan_kibble(Severity::Verbose, "kibble", "kib", kb::col::ndxorange);
    chan_kibble.attach_sink(console_sink);
    Channel chan_memory(Severity::Verbose, "memory", "mem", kb::col::aliceblue);
    chan_memory.attach_sink(console_sink);

    klog(chan_kibble).info("user_size: {}, alignment: {}", sizeof(Data), alignof(Data));

    // Allocate 1MB on the heap
    kb::memory::HeapArea heap(1_MB, &chan_memory);
    // Construct a memory arena using a pool allocator, that can hold 32 instances of Data
    // Pass logging channel to arena so we can log allocations / deallocations and get a shutdown report
    MemoryPool pool("MemPool", heap, 32ul, sizeof(Data), alignof(Data));

    // Show all arenas in the heap area
    heap.debug_show_content();

    // Use the K_NEW macro instead of new to allocate on the pool
    // An initializer list or parentheses with ctor arguments can be concatenated
    auto* d1 = K_NEW(Data, pool){1, 2.3f};
    auto* d2 = K_NEW(Data, pool){4, 5.6f};
    auto* d3 = K_NEW(Data, pool){7, 8.9f};
    auto* d4 = K_NEW(Data, pool){10, 11.12f};

    // Display objects content
    klog(chan_kibble).info("d1: ({},{})", d1->x, d1->y);
    klog(chan_kibble).info("d2: ({},{})", d2->x, d2->y);
    klog(chan_kibble).info("d3: ({},{})", d3->x, d3->y);
    klog(chan_kibble).info("d4: ({},{})", d4->x, d4->y);

    // Delete objects using the K_DELETE macro
    // Here, a few deletions have been omitted. As we're using the VerboseMemoryTracking policy,
    // this leak will be detected and precise information will be given during the arena shutdown report.
    K_DELETE(d1, pool);
    // K_DELETE(d2, pool);
    K_DELETE(d3, pool);
    // K_DELETE(d4, pool);

    // During arena destruction, the shutdown report is emitted on the memory logging channel
    // It should show that two allocations were not matched by a corresponding deallocation,
    // and precise information will be given, which allows us to locate the exact source of the problem.
    // pool.shutdown();

    return 0;
}