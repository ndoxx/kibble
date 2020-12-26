#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include "math/spline.h"

#include <array>
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

void export_bezier(size_t nsamples, const std::string& filename)
{
    math::FixedBezierSpline bez(
        std::array<glm::vec2, 5>{glm::vec2{0.f, 0.f}, {0.5f, 2.f}, {2.5f, 2.5f}, {3.f, 0.5f}, {1.f, 1.f}});

    std::ofstream ofs(filename);
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
}

namespace kb::math
{
template <> struct PointDistance<glm::vec2>
{
    static inline float distance(const glm::vec2& p0, const glm::vec2& p1) { return glm::distance(p0, p1); }
};
} // namespace kb::math

void export_cspline(size_t nsamples, const std::string& filename)
{
    math::HermiteSpline<glm::vec2> spl({{0.f, 0.f}, {0.5f, 5.f}, {5.2f, 5.5f}, {4.f, 4.8f}}, 0.f);
    KLOG("nuclear", 1) << "Spline length is: " << spl.length(0.01f) << std::endl;

    std::ofstream ofs(filename);
    for(size_t ii = 0; ii < nsamples; ++ii)
    {
        float tt = float(ii) / float(nsamples - 1);
        auto val = spl.value(tt);
        auto pri = spl.prime(tt);
        auto sec = spl.second(tt);
        pri = 0.3f * glm::normalize(pri);
        sec = 0.3f * glm::normalize(sec);

        ofs << tt << ' ' << val.x << ' ' << val.y << ' ' << pri.x << ' ' << pri.y << ' ' << sec.x << ' ' << sec.y
            << std::endl;
    }
}

void export_ucspline(size_t nsamples, const std::string& filename)
{
    math::UniformHermiteSpline<glm::vec2> spl({{0.f, 0.f}, {0.5f, 5.f}, {5.2f, 5.5f}, {4.f, 4.8f}}, 64, 0.f);
    KLOG("nuclear", 1) << "Spline length is: " << spl.length(0.01f) << std::endl;

    std::ofstream ofs(filename);
    for(size_t ii = 0; ii < nsamples; ++ii)
    {
        float tt = float(ii) / float(nsamples - 1);
        auto val = spl.value(tt);
        auto pri = spl.prime(tt);
        auto sec = spl.second(tt);
        pri = 0.3f * glm::normalize(pri);
        sec = 0.3f * glm::normalize(sec);

        ofs << tt << ' ' << val.x << ' ' << val.y << ' ' << pri.x << ' ' << pri.y << ' ' << sec.x << ' ' << sec.y
            << std::endl;
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    size_t nsamples = 100;
    // export_bezier(nsamples, "spline.txt");
    // export_cspline(nsamples, "spline.txt");
    export_ucspline(nsamples, "spline.txt");

    return 0;
}