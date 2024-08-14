#include "kibble/cli/terminal.h"

#ifdef __linux__
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace kb
{
namespace cli
{

// [OS-dependent] Retrieve the respective number of columns and rows in the terminal
std::pair<uint32_t, uint32_t> get_terminal_size()
{
#ifdef __linux__
    winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    return {uint32_t(size.ws_col), uint32_t(size.ws_row)};
#else
#error get_terminal_size() not implemented for this OS
#endif
}

} // namespace cli
} // namespace kb