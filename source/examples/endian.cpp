#include "kibble/algorithm/endian.h"

#include <iostream>

using namespace kb;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    uint16_t aa = 0x1234u;
    uint64_t bb = 0x0123456789abcdefULL;
    constexpr uint16_t cc = bswap<uint16_t>(0x1234u);
    constexpr uint64_t dd = bswap(0x0123456789abcdefULL);

    std::cout << std::hex << aa << " -> " << bswap(aa) << std::endl;
    std::cout << std::hex << bb << " -> " << bswap(bb) << std::endl;
    std::cout << std::hex << cc << std::endl;
    std::cout << std::hex << dd << std::endl;

    return 0;
}
