#include "math/statistics.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;
using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    {
        math::Statistics<> stats;

        KLOGN("nuclear") << "Adding integers from 1 to 4" << std::endl;
        for (size_t ii = 1; ii < 5; ++ii)
        {
            stats.push(float(ii));
            KLOG("nuclear", 1) << stats << std::endl;
        }
        KLOGN("nuclear") << "Adding integers from 4 to 1" << std::endl;
        for (size_t ii = 4; ii > 0; --ii)
        {
            stats.push(float(ii));
            KLOG("nuclear", 1) << stats << std::endl;
        }

        KLOGN("nuclear") << "Reset" << std::endl;
        stats.reset();

        KLOGN("nuclear") << "Calculating height statistics" << std::endl;
        std::vector<float> heights = {175.2f, 162.6f, 135.2f, 192.5f, 178.8f, 165.5f, 220.3f};
        stats.run(heights.begin(), heights.end());
        KLOG("nuclear", 1) << stats << "cm" << std::endl;
    }

    {
        math::Statistics<double> stats;

        KLOGN("nuclear") << "Same with doubles" << std::endl;
        std::vector<double> heights = {175.2, 162.6, 135.2, 192.5, 178.8, 165.5, 220.3};
        stats.run(heights.begin(), heights.end());
        KLOG("nuclear", 1) << stats << "cm" << std::endl;
    }

    return 0;
}
