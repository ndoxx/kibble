#include <cstdint>
#include <immintrin.h>
#include <tuple>

namespace kb::morton::impl::bmi2
{

template <typename keyT>
struct Mask2D
{
};
template <>
struct Mask2D<uint32_t>
{
    static constexpr uint32_t k_x_mask = 0x55555555;
    static constexpr uint32_t k_y_mask = 0xAAAAAAAA;
};
template <>
struct Mask2D<uint64_t>
{
    static constexpr uint64_t k_x_mask = 0x5555555555555555;
    static constexpr uint64_t k_y_mask = 0xAAAAAAAAAAAAAAAA;
};

template <typename keyT>
struct Mask3D
{
};
template <>
struct Mask3D<uint32_t>
{
    static constexpr uint32_t k_x_mask = 0x49249249;
    static constexpr uint32_t k_y_mask = 0x92492492;
    static constexpr uint32_t k_z_mask = 0x24924924;
};
template <>
struct Mask3D<uint64_t>
{
    static constexpr uint64_t k_x_mask = 0x9249249249249249;
    static constexpr uint64_t k_y_mask = 0x2492492492492492;
    static constexpr uint64_t k_z_mask = 0x4924924924924924;
};

// clang-format off

uint32_t encode_32_2d(const uint32_t x, const uint32_t y)
{
    return _pdep_u32(x, Mask2D<uint32_t>::k_x_mask) |
           _pdep_u32(y, Mask2D<uint32_t>::k_y_mask);
}
uint64_t encode_64_2d(const uint64_t x, const uint64_t y)
{
    return _pdep_u64(x, Mask2D<uint64_t>::k_x_mask) |
           _pdep_u64(y, Mask2D<uint64_t>::k_y_mask);
}

uint32_t encode_32_3d(const uint32_t x, const uint32_t y, const uint32_t z)
{
    return _pdep_u32(x, Mask3D<uint32_t>::k_x_mask) |
           _pdep_u32(y, Mask3D<uint32_t>::k_y_mask) |
           _pdep_u32(z, Mask3D<uint32_t>::k_z_mask);
}
uint64_t encode_64_3d(const uint64_t x, const uint64_t y, const uint64_t z)
{
    return _pdep_u64(x, Mask3D<uint64_t>::k_x_mask) |
           _pdep_u64(y, Mask3D<uint64_t>::k_y_mask) |
           _pdep_u64(z, Mask3D<uint64_t>::k_z_mask);
}

std::tuple<uint32_t, uint32_t> decode_32_2d(const uint32_t m)
{
    return {
        _pext_u32(m, Mask2D<uint32_t>::k_x_mask), 
        _pext_u32(m, Mask2D<uint32_t>::k_y_mask)
    };
}
std::tuple<uint64_t, uint64_t> decode_64_2d(const uint64_t m)
{
    return {
        _pext_u64(m, Mask2D<uint64_t>::k_x_mask), 
        _pext_u64(m, Mask2D<uint64_t>::k_y_mask)
    };
}

std::tuple<uint32_t, uint32_t, uint32_t> decode_32_3d(const uint32_t m)
{
    return {
        _pext_u32(m, Mask3D<uint32_t>::k_x_mask),
        _pext_u32(m, Mask3D<uint32_t>::k_y_mask),
        _pext_u32(m, Mask3D<uint32_t>::k_z_mask)
    };
}
std::tuple<uint64_t, uint64_t, uint64_t> decode_64_3d(const uint64_t m)
{
    return {
        _pext_u64(m, Mask3D<uint64_t>::k_x_mask),
        _pext_u64(m, Mask3D<uint64_t>::k_y_mask),
        _pext_u64(m, Mask3D<uint64_t>::k_z_mask)
    };
}

// clang-format on

} // namespace kb::morton::impl::bmi2