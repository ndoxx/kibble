#include "assert/assert.h"
#include "logger/stack_trace.h"
#include <cstdio>

namespace detail
{

void assert_redirect()
{
    bool loop = true;
    while (loop)
    {
        printf("What should we do about that?\n  * 0: BREAK\n  * 1: PRINT BACKTRACE\n  * 2: CONTINUE ANYWAY\n  * 3: "
               "EXIT =>[]\n> ");
        switch (getchar())
        {
        case '0':
            K_DEBUG_BREAK();
        case '1':
            kb::printf_backtrace();
            break;
        case '2':
            loop = false;
            break;
        case '3':
            exit(-1);
        }
    }
}

} // namespace detail
