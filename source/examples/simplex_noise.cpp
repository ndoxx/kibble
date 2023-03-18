#include "random/simplex_noise.h"
#include "random/noise_blender.h"
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

using namespace kb;

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    rng::XorShiftEngine rng;
    rng.seed(23456);
    rng::SimplexNoiseGenerator simplex(rng);
    rng::NoiseBlender<rng::SimplexNoiseGenerator> blender(rng);

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
        for (size_t ii = 0; ii < max_grd_2d; ++ii)
        {
            float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_2d - 1);
            for (size_t jj = 0; jj < max_grd_2d; ++jj)
            {
                float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_2d - 1);
                ofs << xx << ' ' << yy << ' ' << simplex(xx, yy) << std::endl;
            }
        }
    }

    {
        std::ofstream ofs("snoise_3d.txt");
        for (size_t ii = 0; ii < max_grd_3d; ++ii)
        {
            float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_3d - 1);
            for (size_t jj = 0; jj < max_grd_3d; ++jj)
            {
                float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_3d - 1);
                for (size_t kk = 0; kk < max_grd_3d; ++kk)
                {
                    float zz = zmin + (zmax - zmin) * float(kk) / float(max_grd_3d - 1);
                    ofs << xx << ' ' << yy << ' ' << zz << ' ' << simplex(xx, yy, zz) << std::endl;
                }
            }
            ofs << std::endl;
        }
    }

    {
        for (size_t ll = 0; ll < max_cuts; ++ll)
        {
            float ww = wmin + (wmax - wmin) * float(ll) / float(max_cuts - 1);
            std::stringstream filename;
            filename << "snoise_4d_" << ll << ".txt";
            std::ofstream ofs(filename.str());
            for (size_t ii = 0; ii < max_grd_4d; ++ii)
            {
                float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_4d - 1);
                for (size_t jj = 0; jj < max_grd_4d; ++jj)
                {
                    float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_4d - 1);
                    for (size_t kk = 0; kk < max_grd_4d; ++kk)
                    {
                        float zz = zmin + (zmax - zmin) * float(kk) / float(max_grd_4d - 1);
                        ofs << xx << ' ' << yy << ' ' << zz << ' ' << simplex(xx, yy, zz, ww) << std::endl;
                    }
                }
                ofs << std::endl;
            }
        }
    }

    {
        std::ofstream ofs("snoise_smooth_2d.txt");
        for (size_t ii = 0; ii < max_grd_2d; ++ii)
        {
            float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_2d - 1);
            for (size_t jj = 0; jj < max_grd_2d; ++jj)
            {
                float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_2d - 1);
                ofs << xx << ' ' << yy << ' ' << blender.smooth_sample_2d(xx, yy, 0.1f) << std::endl;
            }
        }
    }

    {
        std::ofstream ofs("snoise_oct_2d.txt");
        for (size_t ii = 0; ii < max_grd_2d; ++ii)
        {
            float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_2d - 1);
            for (size_t jj = 0; jj < max_grd_2d; ++jj)
            {
                float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_2d - 1);
                ofs << xx << ' ' << yy << ' ' << blender.octave(xx, yy, 5, 0.3f, 0.4f) << std::endl;
            }
        }
    }

    {
        std::ofstream ofs("snoise_marx_2d.txt");
        for (size_t ii = 0; ii < max_grd_2d; ++ii)
        {
            float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_2d - 1);
            for (size_t jj = 0; jj < max_grd_2d; ++jj)
            {
                float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_2d - 1);
                ofs << xx << ' ' << yy << ' ' << blender.marble_x_2d(xx, yy, 5, 10.f, 0.4f) << std::endl;
            }
        }
    }

    {
        std::ofstream ofs("snoise_mary_2d.txt");
        for (size_t ii = 0; ii < max_grd_2d; ++ii)
        {
            float xx = xmin + (xmax - xmin) * float(ii) / float(max_grd_2d - 1);
            for (size_t jj = 0; jj < max_grd_2d; ++jj)
            {
                float yy = ymin + (ymax - ymin) * float(jj) / float(max_grd_2d - 1);
                ofs << xx << ' ' << yy << ' ' << blender.marble_y_2d(xx, yy, 5, 10.f, 0.4f) << std::endl;
            }
        }
    }
    return 0;
}
