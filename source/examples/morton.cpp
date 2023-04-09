#include "math/morton.h"
#include <iostream>
#include <bitset>

using namespace kb;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    int32_t xymin = -4;
    int32_t xymax = 4;

    for (int32_t xx = xymin; xx <= xymax; ++xx)
    {
        for (int32_t yy = xymin; yy <= xymax; ++yy)
        {
            uint32_t xxu = uint32_t(xx - xymin);
            uint32_t yyu = uint32_t(yy - xymin);
            auto m = morton::encode(xxu, yyu);
            auto&& [xd, yd] = morton::decode_2d(m);
            int32_t xds = int32_t(xd) + xymin;
            int32_t yds = int32_t(yd) + xymin;
            std::cout << xx << ", " << yy << " -> " << std::bitset<32>(m) << " -> " << xds << ", " << yds << std::endl;
        }
    }

    return 0;
}