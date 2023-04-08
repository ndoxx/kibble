#include "math/morton.h"
#include <iostream>
#include <bitset>

using namespace kb;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    int16_t xymin = -4;
    int16_t xymax = 4;

    for (int16_t xx = xymin; xx <= xymax; ++xx)
    {
        for (int16_t yy = xymin; yy <= xymax; ++yy)
        {
            uint16_t xxu = uint16_t(xx - xymin);
            uint16_t yyu = uint16_t(yy - xymin);
            auto m = morton::encode_32(xxu, yyu);
            auto&& [xd, yd] = morton::decode_2d_32(m);
            int16_t xds = int16_t(xd) + xymin;
            int16_t yds = int16_t(yd) + xymin;
            std::cout << xx << ", " << yy << " -> " << std::bitset<32>(m) << " -> " << xds << ", " << yds << std::endl;
        }
    }

    return 0;
}