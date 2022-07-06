#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "memory/heap_area.h"
#include "thread/job/config.h"
#include "thread/job/daemon.h"
#include "thread/job/job_system.h"
#include "time/clock.h"
#include "time/instrumentation.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace kb;
using namespace kb::th;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(create_channel("thread", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    th::JobSystemScheme scheme;
    scheme.max_workers = 0;
    scheme.max_stealing_attempts = 16;
    scheme.scheduling_algorithm = th::SchedulingAlgorithm::round_robin;
    scheme.persistence_file = "nuclear.jpp";

    // The job system needs some pre-allocated memory for the job pool.
    // Fortunately, it can evaluate the memory requirements, so we don't have to guess.
    memory::HeapArea area(th::JobSystem::get_memory_requirements());

    auto *js = new th::JobSystem(area, scheme);
    auto *ds = new th::DaemonScheduler(*js);

    // Job system profiling (compile )
    auto *session = new InstrumentationSession("nuclear.json");
    js->set_instrumentation_session(session);

    struct Message
    {
        std::string message;
        float interval_ms;
    };

    std::vector<Message> msgs = {{"hello", 100.f}, {"salut", 200.f}, {"sunt eu", 500.f}, {"un haiduc", 1000.f}};

    for (const auto &msg : msgs)
    {
        th::JobMetadata meta;
        meta.set_profile_data(msg.message);
        meta.label = H_(msg.message);
        meta.worker_affinity = th::WORKER_AFFINITY_ASYNC;

        th::SchedulingData sd;
        sd.interval_ms = msg.interval_ms;

        ds->create(
            [msg]() {
                KLOG("nuclear", 1) << msg.message << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            },
            sd, meta);
    }

    auto frame = std::chrono::microseconds(16670);
    for (size_t ii = 0; ii < 100; ++ii)
    {
        microClock clk;

        for (size_t jj = 0; jj < 100; ++jj)
        {
            th::JobMetadata meta;
            meta.set_profile_data("A job");
            meta.label = "A job"_h;
            meta.worker_affinity = th::WORKER_AFFINITY_ANY;

            auto tsk = js->create_task([]() { std::this_thread::sleep_for(std::chrono::microseconds(500)); }, meta);
            tsk.schedule();
        }

        ds->update(float(frame.count()) / 1000.f);
        js->wait();
        auto dur = clk.get_elapsed_time();
        auto sleep_time = frame - dur;
        std::this_thread::sleep_for(sleep_time);
    }

    delete ds;
    delete js;
    delete session;

    return 0;
}
