#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include "math/catenary.h"

#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <limits>
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

    float scale = 1.f;

#define C 0
#if C == 0
    // x1<x2 && y1<y2
    float x1 = scale * 0.5f;
    float y1 = scale * 1.6f;
    float x2 = scale * 1.5f;
    float y2 = scale * 2.0f;
#elif C == 1
    // x1<x2 && y1>y2
    float x1 = scale * 0.5f;
    float y1 = scale * 2.0f;
    float x2 = scale * 1.5f;
    float y2 = scale * 1.6f;
#elif C == 2
    // x1>x2 && y1<y2
    float x1 = scale * 1.5f;
    float y1 = scale * 1.6f;
    float x2 = scale * 0.5f;
    float y2 = scale * 2.0f;
#elif C == 3
    // x1>x2 && y1>y2
    float x1 = scale * 1.5f;
    float y1 = scale * 2.0f;
    float x2 = scale * 0.5f;
    float y2 = scale * 1.6f;
#else
    float x1 = -10.f;
    float y1 = 16.f;
    float x2 = 5.f;
    float y2 = 8.f;
#endif
    size_t nsamples = 100;
    size_t ncats = 4;

    std::vector<math::Catenary> cats;
    for(size_t ii = 0; ii < ncats; ++ii)
    {
        float tt = float(ii) / float(ncats - 1);
        float s = scale * std::lerp(1.1f, 3.1f, tt);
        KLOGN("nuclear") << "s=" << s << " v=" << y2 - y1 << " h=" << x2 - x1 << std::endl;
        cats.emplace_back(x1, y1, x2, y2, s);
    }

    // Multiple catenary curves with same anchor points but different lengths
    {
        std::ofstream ofs("catenary.txt");
        for(size_t ii = 0; ii < nsamples; ++ii)
        {
            float tt = float(ii) / float(nsamples - 1);
            float xx = (1.f - tt) * x1 + tt * x2;
            ofs << xx << ' ';
            for(size_t jj = 0; jj < ncats; ++jj)
                ofs << cats[jj].value(xx) << ' ';
            ofs << std::endl;
        }
    }
    // Single catenary curve, two parameterizations
    {
        std::ofstream ofs("catenary_alp.txt");
        for(size_t ii = 0; ii < nsamples; ++ii)
        {
            float tt = float(ii) / float(nsamples - 1);
            float xx = (1.f - tt) * x1 + tt * x2;
            float rt = cats[ncats - 1].arclen_remap(tt);
            ofs << xx << ' ' << cats[ncats - 1].value(xx) << ' ' << rt << ' ' << cats[ncats - 1].value(rt) << std::endl;
        }
    }
    // Catenary and its tangent vectors
    {
        std::ofstream ofs("catenary_der.txt");
        for(size_t ii = 0; ii < nsamples; ++ii)
        {
            float tt = float(ii) / float(nsamples - 1);
            float xx = (1.f - tt) * x1 + tt * x2;
            float der = cats[ncats - 1].prime(xx);
            glm::vec2 tangent{1.f, der};
            tangent = glm::normalize(tangent);
            ofs << xx << ' ' << cats[ncats - 1].value(xx) << ' ' << tangent.x << ' ' << tangent.y << std::endl;
        }
    }
    return 0;
}