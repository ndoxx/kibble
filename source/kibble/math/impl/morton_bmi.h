#pragma once

#include <cstdint>
#include <immintrin.h>
#include <tuple>

namespace kb::morton
{
namespace bmi2
{

inline uint32_t pdep(uint32_t source, uint32_t mask) noexcept
{
    return _pdep_u32(source, mask);
}
inline uint64_t pdep(uint64_t source, uint64_t mask) noexcept
{
    return _pdep_u64(source, mask);
}
inline uint32_t pext(uint32_t source, uint32_t mask) noexcept
{
    return _pext_u32(source, mask);
}
inline uint64_t pext(uint64_t source, uint64_t mask) noexcept
{
    return _pext_u64(source, mask);
}
} // namespace bmi2

namespace impl2d
{

template <typename keyT>
struct Mask
{
};
template <>
struct Mask<uint32_t>
{
    static constexpr uint32_t k_x_mask = 0x55555555;
    static constexpr uint32_t k_y_mask = 0xAAAAAAAA;
};
template <>
struct Mask<uint64_t>
{
    static constexpr uint64_t k_x_mask = 0x5555555555555555;
    static constexpr uint64_t k_y_mask = 0xAAAAAAAAAAAAAAAA;
};

template <typename keyT, typename coordT>
inline keyT encode(const coordT x, const coordT y)
{
    return static_cast<keyT>(bmi2::pdep(static_cast<keyT>(x), Mask<keyT>::k_x_mask) |
                             bmi2::pdep(static_cast<keyT>(y), Mask<keyT>::k_y_mask));
}

template <typename keyT, typename coordT>
inline std::tuple<coordT, coordT> decode(const keyT m)
{
    return {static_cast<coordT>(bmi2::pext(m, Mask<keyT>::k_x_mask)),
            static_cast<coordT>(bmi2::pext(m, Mask<keyT>::k_y_mask))};
}

} // namespace impl2d

namespace impl3d
{

template <typename keyT>
struct Mask
{
};
template <>
struct Mask<uint32_t>
{
    static constexpr uint32_t k_x_mask = 0x49249249;
    static constexpr uint32_t k_y_mask = 0x92492492;
    static constexpr uint32_t k_z_mask = 0x24924924;
};
template <>
struct Mask<uint64_t>
{
    static constexpr uint64_t k_x_mask = 0x9249249249249249;
    static constexpr uint64_t k_y_mask = 0x2492492492492492;
    static constexpr uint64_t k_z_mask = 0x4924924924924924;
};

template <typename keyT, typename coordT>
inline keyT encode(const coordT x, const coordT y, const coordT z)
{
    return static_cast<keyT>(bmi2::pdep(static_cast<keyT>(x), Mask<keyT>::k_x_mask) |
                             bmi2::pdep(static_cast<keyT>(y), Mask<keyT>::k_y_mask) |
                             bmi2::pdep(static_cast<keyT>(z), Mask<keyT>::k_z_mask));
}

template <typename keyT, typename coordT>
inline std::tuple<coordT, coordT, coordT> decode(const keyT m)
{
    return {static_cast<coordT>(bmi2::pext(m, Mask<keyT>::k_x_mask)),
            static_cast<coordT>(bmi2::pext(m, Mask<keyT>::k_y_mask)),
            static_cast<coordT>(bmi2::pext(m, Mask<keyT>::k_z_mask))};
}

} // namespace impl3d
} // namespace kb::morton