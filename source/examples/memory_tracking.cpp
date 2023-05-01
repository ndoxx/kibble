#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/policies/stack_trace_policy.h"
#include "logger2/policies/uid_filter.h"
#include "logger2/sinks/console_sink.h"
#include "logger2/sinks/file_sink.h"

#include "memory/heap_area.h"
#include "memory/memory.h"
#include "memory/memory_utils.h"
#include "memory/policy.h"
#include "memory/pool_allocator.h"

using namespace kb::log;
using namespace kb;

/*
    The PoolAllocator allocation policy makes the MemoryArena a memory pool.
    Here we use a VerboseMemoryTracking policy to fully track allocations.
    In a retail build, we could use the NoMemoryTracking policy instead, which
    completly suppresses memory tracking and the associated overhead.
*/
using MemoryPool =
    kb::memory::MemoryArena<kb::memory::PoolAllocator, kb::memory::policy::SingleThread,
                            kb::memory::policy::SimpleBoundsChecking, kb::memory::policy::NoMemoryTagging,
                            kb::memory::policy::VerboseMemoryTracking>;

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

    // Allocate 1MB on the heap
    kb::memory::HeapArea heap(1_MB, &chan_memory);
    // Construct a memory arena using a pool allocator, that can hold 32 instances of Data
    // Pass logging channel to arena so we can log allocations / deallocations and get a shutdown report
    MemoryPool pool("MemPool", heap, sizeof(Data) + MemoryPool::DECORATION_SIZE, 32ul);

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