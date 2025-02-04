#include "kibble/cli/terminal.h"
#include "kibble/platform/platform.h"

#if defined(K_PLATFORM_LINUX)
#include <sys/ioctl.h>
#include <unistd.h>
#elif defined(K_PLATFORM_WINDOWS)
#if defined(K_COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#endif
#include <windows.h>
#if defined(K_COMPILER_CLANG)
#pragma clang diagnostic pop
#endif
#endif

namespace kb
{
namespace cli
{

// [OS-dependent] Retrieve the respective number of columns and rows in the terminal
std::pair<uint32_t, uint32_t> get_terminal_size()
{
#if defined(K_PLATFORM_LINUX)

    winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    return {uint32_t(size.ws_col), uint32_t(size.ws_row)};

#elif defined(K_PLATFORM_WINDOWS)

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns, rows;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return {uint32_t(columns), uint32_t(rows)};

#else
#error get_terminal_size() not implemented for this OS
#endif
}

void enable_terminal_ANSI_support()
{
#if defined(K_PLATFORM_WINDOWS)
    // Enable ANSI escape codes
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

} // namespace cli
} // namespace kb