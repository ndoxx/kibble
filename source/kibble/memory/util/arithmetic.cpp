#include "arithmetic.h"

#include "fmt/core.h"
#include <cstdint>
#include <iomanip>

namespace kb::memory::utils
{

constexpr std::string_view size_unit_suffix(int exponent)
{
    switch (exponent)
    {
    case 0:
        return "B";
    case 1:
        return "kB";
    case 2:
        return "MB";
    case 3:
        return "GB";
    case 4:
        return "TB";
    default:
        return "??";
    }
}

constexpr int k_max_suffix = 4;

std::size_t round_up(std::size_t base, std::size_t multiple)
{
    if (multiple == 0)
        return base;

    std::size_t remainder = base % multiple;
    if (remainder == 0)
        return base;

    return base + multiple - remainder;
}

std::string human_size(std::size_t bytes)
{
    int ii = 0;
    double d_bytes = double(bytes);

    if (bytes > 1024)
    {
        for (ii = 0; (bytes / 1024) > 0 && ii < k_max_suffix; ii++, bytes /= 1024)
            d_bytes = double(bytes) / 1024.0;
    }

    return fmt::format("{:.2f}{}", d_bytes, size_unit_suffix(ii));
}

} // namespace kb::memory::utils