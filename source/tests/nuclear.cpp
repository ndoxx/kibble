#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

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

constexpr const int kfac[] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800};

int choose(int nn, int ii) { return (ii <= nn) ? kfac[nn] / (kfac[ii] * kfac[nn - ii]) : 0; }
int parity(int ii) { return ((ii % 2) ? -1 : 1); }

std::vector<glm::vec2> bezier_coefficients(const std::vector<glm::vec2>& control)
{
    std::vector<glm::vec2> coeffs;
    coeffs.resize(control.size());

    const int nn = int(control.size());
    int prod = 1;
    for(int jj = 0; jj < nn; ++jj)
    {
        // Product from m=0 to j-1 of (n-m)
        prod *= (jj > 0) ? nn - jj : 1;

        glm::vec2 sum(0.f);
        for(int ii = 0; ii <= jj; ++ii)
        {
            int comb = (prod * parity(ii + jj)) / (kfac[ii] * kfac[jj - ii]);
            sum += float(comb) * control[size_t(ii)];
        }
        coeffs[size_t(jj)] = sum;
    }
    return coeffs;
}

std::vector<glm::vec2> bezier_tangent_coefficients(const std::vector<glm::vec2>& control)
{
    std::vector<glm::vec2> coeffs;
    coeffs.resize(control.size());

    const int nn = int(control.size()) - 1;
    int prod = 1;
    for(int jj = 0; jj < nn; ++jj)
    {
        // Product from m=0 to j of (n-m)
        prod *= nn - jj;

        glm::vec2 sum(0.f);
        for(int ii = 0; ii <= jj; ++ii)
        {
            int comb = (prod * parity(ii + jj)) / (kfac[ii] * kfac[jj - ii]);
            sum += float(comb) * (control[size_t(ii + 1)] - control[size_t(ii)]);
        }
        coeffs[size_t(jj)] = sum;
    }
    return coeffs;
}

glm::vec2 bezier_interpolate(float tt, const std::vector<glm::vec2>& coeffs)
{
    glm::vec2 sum(0.f);
    float ttpow = 1.f;
    for(size_t ii = 0; ii < coeffs.size(); ++ii)
    {
        sum += ttpow * coeffs[ii];
        ttpow *= tt;
    }
    return sum;
}

glm::vec2 bezier_tangent(float tt, const std::vector<glm::vec2>& control)
{
    glm::vec2 sum(0.f);
    const int nn = int(control.size()) - 1;
    for(int ii = 0; ii <= nn; ++ii)
    {
        int nchoosei = kfac[nn] / (kfac[ii] * kfac[nn - ii]);
        float omtt = 1.f - tt;
        float coeff = float(ii) * float(std::pow(tt, ii - 1)) * float(std::pow(omtt, nn - ii)) -
                      float(nn - ii) * float(std::pow(tt, ii)) * float(std::pow(omtt, nn - ii - 1));
        sum += float(nchoosei) * coeff * control[size_t(ii)];
    }
    return sum;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    size_t nsamples = 100;

    std::vector<glm::vec2> control = {{0.f, 0.f}, {0.5f, 2.f}, {2.5f, 2.5f}, {3.f, 0.5f}, {1.f, 1.f}};
    auto coeffs = bezier_coefficients(control);
    auto tcoeffs = bezier_tangent_coefficients(control);

    std::ofstream ofs("bezier.txt");
    for(size_t ii = 0; ii < nsamples; ++ii)
    {
        float tt = float(ii) / float(nsamples - 1);
        auto val = bezier_interpolate(tt, coeffs);
        auto tval = bezier_interpolate(tt, tcoeffs);
        // auto tval = bezier_tangent(tt, control);
        tval = 0.3f * glm::normalize(tval);
        ofs << tt << ' ' << val.x << ' ' << val.y << ' ' << tval.x << ' ' << tval.y << std::endl;
    }

    return 0;
}