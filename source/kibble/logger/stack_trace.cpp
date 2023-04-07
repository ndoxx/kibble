#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logger/stack_trace.h"

namespace kb
{
#ifdef __linux__
std::string get_backtrace()
{
    void* bt[1024];
    int bt_size = backtrace(bt, 1024);
    char** bt_syms = backtrace_symbols(bt, bt_size);

    std::stringstream ss;
    for (int ii = 1; ii < bt_size; ++ii)
    {
        /*Dl_info  DlInfo;
        if (dladdr(bt[ii], &DlInfo) != 0)
            ss << DlInfo.dli_sname << std::endl;*/
        // full_write(STDERR_FILENO, DlInfo.dli_sname, strlen(DlInfo.dli_sname));

        ss << bt_syms[ii] << std::endl;
    }
    free(bt_syms);

    return ss.str();
}
#endif

void print_backtrace()
{
    std::cout << get_backtrace();
}

void printf_backtrace()
{
    printf("%s", get_backtrace().c_str());
}

} // namespace kb
