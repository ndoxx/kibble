#pragma once

#include <cstddef>

namespace kb::memory::literals
{

/**
 * @brief Convert a size in bytes to a size in bytes (does nothing)
 *
 */
constexpr inline std::size_t operator"" _B(unsigned long long size)
{
    return size;
}

/**
 * @brief Convert a size in kilobytes to a size in bytes
 *
 */
constexpr inline std::size_t operator"" _kB(unsigned long long size)
{
    return 1024 * size;
}

/**
 * @brief Convert a size in megabytes to a size in bytes
 *
 */
constexpr inline std::size_t operator"" _MB(unsigned long long size)
{
    return 1048576 * size;
}

/**
 * @brief Convert a size in gigabytes to a size in bytes
 *
 */
constexpr inline std::size_t operator"" _GB(unsigned long long size)
{
    return 1073741824 * size;
}

} // namespace kb::memory::literals