#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "random/simplex_noise.h"
#include "random/xor_shift.h"

#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    rng::XorShiftEngine rng;
    rng.seed(23456);
    rng::SimplexNoiseGenerator simplex(rng);

    size_t max_grd_2d = 100;
    size_t max_grd_3d = 20;
    size_t max_grd_4d = 20;
    size_t max_cuts = 4;
    float xmin = -2.f;
    float xmax = 2.f;
    float ymin = -2.f;
    float ymax = 2.f;
    float zmin = -2.f;
    float zmax = 2.f;
    float wmin = -0.1f;
    float wmax = 0.1f;

    {
        std::ofstream ofs("snoise_2d.txt");
        for(size_t ii = 0; ii < max_grd_2d; ++ii)
        {
            float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_2d - 1);
            for(size_t jj = 0; jj < max_grd_2d; ++jj)
            {
                float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_2d - 1);
                ofs << xx << ' ' << yy << ' ' << simplex(xx, yy) << std::endl;
            }
        }
    }

    {
        std::ofstream ofs("snoise_3d.txt");
        for(size_t ii = 0; ii < max_grd_3d; ++ii)
        {
            float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_3d - 1);
            for(size_t jj = 0; jj < max_grd_3d; ++jj)
            {
                float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_3d - 1);
                for(size_t kk = 0; kk < max_grd_3d; ++kk)
                {
                    float zz = zmin + (zmax - zmin) * float(kk) / float(max_grd_3d - 1);
                    ofs << xx << ' ' << yy << ' ' << zz << ' ' << simplex(xx, yy, zz) << std::endl;
                }
            }
            ofs << std::endl;
        }
    }

    {
        for(size_t ll = 0; ll < max_cuts; ++ll)
        {
            float ww = wmin + (wmax - wmin) * float(ll) / float(max_cuts - 1);
            std::stringstream filename;
            filename << "snoise_4d_" << ll << ".txt";
            std::ofstream ofs(filename.str());
            for(size_t ii = 0; ii < max_grd_4d; ++ii)
            {
                float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_4d - 1);
                for(size_t jj = 0; jj < max_grd_4d; ++jj)
                {
                    float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_4d - 1);
                    for(size_t kk = 0; kk < max_grd_4d; ++kk)
                    {
                        float zz = zmin + (zmax - zmin) * float(kk) / float(max_grd_4d - 1);
                        ofs << xx << ' ' << yy << ' ' << zz << ' ' << simplex(xx, yy, zz, ww) << std::endl;
                    }
                }
                ofs << std::endl;
            }
        }
    }
    return 0;
}
