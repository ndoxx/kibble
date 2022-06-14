#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include "memory/heap_area.h"
#include "memory/memory.h"
#include "memory/atomic_pool_allocator.h"

#include <vector>
#include <thread>

using namespace kb;
using namespace kb::memory;


void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

struct Job
{
    size_t a = 0;
    size_t b = 0;
};

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    HeapArea area(1_MB);

    constexpr size_t k_max_jobs = 128;
    constexpr size_t k_max_threads = 4;
    constexpr size_t k_cache_line_size = 64;
    constexpr size_t k_job_max_align = k_cache_line_size - 1;
    constexpr size_t k_job_node_size = sizeof(Job) + k_job_max_align;

    using PoolArena = memory::MemoryArena<AtomicPoolAllocator<k_max_jobs * k_max_threads>,
                                          memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                                          memory::policy::NoMemoryTagging, memory::policy::NoMemoryTracking>;

    PoolArena job_pool;

    job_pool.init(area, k_job_node_size + PoolArena::DECORATION_SIZE, "JobPool");

    std::vector<Job *> jobs;
    for (size_t ii = 0; ii < 10; ++ii)
    {
        Job *job = K_NEW_ALIGN(Job, job_pool, k_cache_line_size);
        job->a = ii;
        job->b = ii + 1;
        jobs.push_back(job);
    }

    for (Job *job : jobs)
    {
        KLOG("nuclear",1) << job->a << '/' << job->b << std::endl;
        K_DELETE(job, job_pool);
    }

    return 0;
}
