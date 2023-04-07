#include "random/uuid.h"
#include "random/xor_shift.h"

#include <iostream>

using namespace kb;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    UUIDv4::UUIDGenerator<rng::XorShiftEngine> gen;

    for (size_t ii = 0; ii < 10; ++ii)
    {
        std::cout << gen() << std::endl;
    }

    return 0;
}
