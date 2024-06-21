#include "math/morton_glm.h" // For the GLM wrappers
#include <glm/gtx/string_cast.hpp>

#include "fmt/core.h"

using namespace kb;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // * Display 2D-Morton encoding - decoding for a small 8x8 grid

    // Morton encoding only supports positive (unsigned) arguments
    // We'll see how to get around this limitation
    int32_t grid_min = -4;
    int32_t grid_max = 4;

    for (int32_t xx = grid_min; xx <= grid_max; ++xx)
    {
        for (int32_t yy = grid_min; yy <= grid_max; ++yy)
        {
            // Translate frame so that all coordinates are positive, and encode
            uint32_t xxu = uint32_t(xx - grid_min);
            uint32_t yyu = uint32_t(yy - grid_min);
            auto m = morton::encode(xxu, yyu);
            // Now decode, translate back to original frame
            auto&& [xd, yd] = morton::decode_2d(m);
            int32_t xds = int32_t(xd) + grid_min;
            int32_t yds = int32_t(yd) + grid_min;
            // Max argument = 8 (4 bits), 2D -> 2 bits interleaved -> 8 bits total
            fmt::println("({},{}) -> 0b{:08b} -> ({},{})", xx, yy, m, xds, yds);
        }
    }

    // * GLM wrappers
    // All integral glm vectors of dimension 2 and 3 are supported
    fmt::println("{}", morton::encode(glm::i32vec2{48, 231}));
    fmt::println("{}", glm::to_string(morton::decode<glm::i32vec2>(44330u)));

    fmt::println("{}", morton::encode(glm::u64vec2{48, 231}));
    fmt::println("{}", glm::to_string(morton::decode<glm::u64vec2>(44330ul)));

    fmt::println("{}", morton::encode(glm::i64vec3{48, 231, 72}));
    fmt::println("{}", glm::to_string(morton::decode<glm::i64vec3>(5871762ul)));

    // morton::encode(glm::vec2{48, 231}); // Does not compile, glm::vec2::value_type (float) is not integral

    return 0;
}