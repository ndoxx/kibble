#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include "math/spline.h"

#include <cmath>
#include <fstream>
#include <glm/glm.hpp>
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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    size_t nsamples = 100;

    // We should end up with {{0.f, 0.f}, {0.5f, 2.f}, {2.5f, 2.5f}, {3.f, 0.5f}, {1.f, 1.f}}
    // math::BezierSpline<glm::vec2> bez({{0.f, 0.f}, {2.5f, 2.5f}, {3.f, 0.5f}, {99.f, 99.f}});
    // bez.add({1.f, 1.f});
    // bez.insert(1, {0.5f, 2.f});
    // bez.remove(4);

    // math::BasicSpline<glm::vec2>* bez = new math::BezierSpline<glm::vec2>({{0.f, 0.f}, {0.5f, 2.f}, {2.5f, 2.5f}, {3.f, 0.5f}, {1.f, 1.f}});
    
    math::BezierSpline<glm::vec2> bez({{0.f, 0.f}, {0.5f, 2.f}, {2.5f, 2.5f}, {3.f, 0.5f}, {1.f, 1.f}});

    std::ofstream ofs("bezier.txt");
    for(size_t ii = 0; ii < nsamples; ++ii)
    {
        float tt = float(ii) / float(nsamples - 1);
        auto val = bez.value(tt);
        auto pri = bez.prime(tt);
        auto sec = bez.second(tt);
        pri = 0.3f * glm::normalize(pri);
        sec = 0.3f * glm::normalize(sec);

        ofs << tt << ' ' << val.x << ' ' << val.y << ' ' << pri.x << ' ' << pri.y << ' ' << sec.x << ' ' << sec.y
            << std::endl;
    }

    return 0;
}