#pragma once

#ifndef K_DEBUG
#define K_DEBUG
#endif

#include "argparse/argparse.h"
#include "logger2/logger.h"
#include "memory/heap_area.h"
#include "thread/job/job_system.h"
#include "time/clock.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

void show_error_and_die(kb::ap::ArgParse& parser, const kb::log::Channel& chan);

std::pair<float, float> stats(const std::vector<long> durations);

void show_statistics(kb::milliClock& clk, long serial_dur_ms, const kb::log::Channel& chan);

template <typename T, typename Iter>
void random_fill(Iter start, Iter end, T min, T max, uint64_t seed = 0)
{
    static std::default_random_engine eng(seed);
    std::uniform_int_distribution<T> dist(min, max);
    std::generate(start, end, [&]() { return dist(eng); });
}

class JobExample
{
public:
    JobExample() = default;
    virtual ~JobExample() = default;

    int run(int argc, char** argv);
    virtual int impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan) = 0;
};

// Entry point
#define JOB_MAIN(EXAMPLE_CLASS)                                                                                        \
    int main(int argc, char** argv)                                                                                    \
    {                                                                                                                  \
        JobExample* app = nullptr;                                                                                     \
                                                                                                                       \
        try                                                                                                            \
        {                                                                                                              \
            app = new EXAMPLE_CLASS();                                                                                 \
        }                                                                                                              \
        catch (...)                                                                                                    \
        {                                                                                                              \
            return -1;                                                                                                 \
        }                                                                                                              \
                                                                                                                       \
        int result = app->run(argc, argv);                                                                             \
        delete app;                                                                                                    \
                                                                                                                       \
        return result;                                                                                                 \
    }

/* USAGE:

#include "harness/job_example.h"

using namespace kb;

class JobExampleImpl: public JobExample
{
public:
    int impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan) override;
};

JOB_MAIN(JobExampleImpl);

int JobExampleImpl::impl(size_t nexp, size_t njobs, kb::th::JobSystem& js, const kb::log::Channel& chan)
{
    // Example app implementation
}

*/