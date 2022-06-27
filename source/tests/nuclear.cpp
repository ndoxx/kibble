#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "thread/job/config.h"
#include "thread/job/job_graph.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace kb;
using namespace kb::th;

struct Job
{
    using JobNode = ProcessNode<k_max_parent_jobs, k_max_child_jobs>;
    JobNode node;

    inline void add_child(Job *job)
    {
        node.connect(job->node);
    }

    inline void add_parent(Job *job)
    {
        job->node.connect(node);
    }

    inline bool is_ready() const
    {
        return node.is_ready();
    }

    inline bool is_processed() const
    {
        return node.is_processed();
    }

    inline void mark_processed()
    {
        node.mark_processed();
    }
};

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("memory", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

void display(const std::vector<Job *> &jobs)
{
    for (auto &&job : jobs)
    {
        KLOG("nuclear", 1) << job->is_ready() << '/' << job->is_processed() << ' ';
    }
    KLOG("nuclear", 1) << std::endl;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    std::vector<Job *> jobs;
    for (size_t ii = 0; ii < 7; ++ii)
        jobs.push_back(new Job);

    jobs[0]->add_child(jobs[2]);
    jobs[0]->add_child(jobs[3]);
    jobs[1]->add_child(jobs[4]);
    jobs[5]->add_parent(jobs[2]);
    jobs[5]->add_parent(jobs[3]);
    jobs[6]->add_parent(jobs[4]);
    jobs[6]->add_parent(jobs[5]);

    auto done = [end_job = jobs[6]]() { return end_job->is_processed(); };

    std::vector<size_t> order(7, 0);
    std::iota(order.begin(), order.end(), 0);
    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(order), std::end(order), rng);

    display(jobs);
    while (!done())
    {
        for (size_t idx : order)
        {
            auto *job = jobs[idx];
            if (job->is_ready() && !job->is_processed())
            {
                job->mark_processed();
                KLOG("nuclear", 1) << "Processing job #" << idx << std::endl;
                display(jobs);
            }
        }
    }

    for (Job *job : jobs)
        delete job;

    return 0;
}
