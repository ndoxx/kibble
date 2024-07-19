#ifndef K_ENABLE_ASSERT
#define K_ENABLE_ASSERT
#endif
#include "assert/assert.h"

__attribute__((optnone)) int bar(int x)
{
    K_ASSERT(x < 10, "x should be less than 10, but it's {}", x);
    return x * x;
}

__attribute__((optnone)) int foo(int x)
{
    return bar(x) + bar(-x);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // int x = foo(2);
    int x = foo(3);
    // int x = foo(125);
    // K_ASSERT(x == 8, "x should be 8");
    K_ASSERT(x == 8, "x should be 8, but it's {}", x);
    // K_CHECK(x == 8, "x should be 8, but it's {}", x);
    // K_FAIL("too bad, buddy!");
    // K_FAIL("too bad, {}!", "buddy");

    fmt::println("x: {}", x);

    return 0;
}