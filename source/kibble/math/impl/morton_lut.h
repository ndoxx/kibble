#pragma once

#include <array>
#include <cstdint>
#include <tuple>

namespace kb::morton
{

namespace impl2d
{

namespace detail
{
template <typename retT>
static constexpr retT mask_8()
{
    return retT(0x000000FF);
}

// Inspired by the artricle: Integer Dilation and Contraction for Quadtrees and Octrees (Leo Stocco & Gunther Schrack)
static constexpr uint32_t dilate_masks_32[6] = {0xFFFFFFFF, 0x0000FFFF, 0x00FF00FF, 0x0F0F0F0F, 0x33333333, 0x55555555};

static constexpr uint32_t gen_dilate(uint32_t a, uint32_t offset)
{
    for (int ii = 1; ii < 6; ++ii)
        a = (a | (a << (4 * sizeof(uint32_t) >> (ii - 1)))) & dilate_masks_32[ii];

    return a << offset;
}

static constexpr uint32_t gen_contract(uint32_t a, uint32_t offset)
{
    a = a >> offset;
    a = a & dilate_masks_32[5];
    for (int ii = 0; ii < 5; ++ii)
        a = (a | (a >> (1 << ii))) & dilate_masks_32[4 - ii];

    return a;
}

template <typename T, std::size_t SIZE>
static constexpr std::array<T, SIZE> make_dilation_LUT(uint32_t offset)
{
    std::array<T, SIZE> result{};
    for (T ii = 0; ii < T(SIZE); ++ii)
        result[ii] = T(gen_dilate(uint32_t(ii), offset));

    return result;
}

template <typename T, std::size_t SIZE>
static constexpr std::array<T, SIZE> make_contraction_LUT(uint32_t offset)
{
    std::array<T, SIZE> result{};
    for (std::size_t ii = 0; ii < SIZE; ++ii)
        result[ii] = T(gen_contract(uint32_t(ii), offset));

    return result;
}

class LUT
{
public:
    static constexpr std::array<uint16_t, 256> dilation_x = make_dilation_LUT<uint16_t, 256>(0);
    static constexpr std::array<uint16_t, 256> dilation_y = make_dilation_LUT<uint16_t, 256>(1);
    static constexpr std::array<uint8_t, 256> contraction_x = make_contraction_LUT<uint8_t, 256>(0);
    static constexpr std::array<uint8_t, 256> contraction_y = make_contraction_LUT<uint8_t, 256>(1);

    // HELPER METHODE for LUT decoding
    template <typename keyT, typename coordT>
    static inline coordT decode(const keyT m, const uint8_t* LUT, const unsigned int startshift)
    {
        keyT a = 0;
        unsigned int loops = sizeof(keyT);
        for (unsigned int ii = 0; ii < loops; ++ii)
            a |= keyT(LUT[(m >> ((ii * 8) + startshift)) & mask_8<keyT>()] << (4 * ii));

        return static_cast<coordT>(a);
    }
};
} // namespace detail

// ENCODE 2D Morton code: Pre-shifted LookUpTable
template <typename keyT, typename coordT>
inline keyT encode(const coordT x, const coordT y)
{
    keyT answer = 0;
    for (coordT ii = sizeof(coordT); ii > 0; --ii)
    {
        coordT shift = (ii - 1) * 8;
        answer = answer << 16 | detail::LUT::dilation_y[(y >> shift) & coordT(detail::mask_8<keyT>())] |
                 detail::LUT::dilation_x[(x >> shift) & coordT(detail::mask_8<keyT>())];
    }
    return answer;
}

// DECODE 2D Morton code : Shifted LUT
template <typename keyT, typename coordT>
inline std::tuple<coordT, coordT> decode(const keyT m)
{
    return {detail::LUT::decode<keyT, coordT>(m, &detail::LUT::contraction_x[0], 0),
            detail::LUT::decode<keyT, coordT>(m, &detail::LUT::contraction_y[0], 0)};
}

} // namespace impl2d

namespace impl3d
{
namespace detail
{
template <typename retT>
static constexpr retT mask_8()
{
    return retT(0x000000FF);
}
template <typename retT>
static constexpr retT mask_9()
{
    return retT(0x000001FF);
}

static constexpr uint32_t dilate_masks_32[4] = {0x030000FF, 0x0300F00F, 0x030C30C3, 0x09249249};

static constexpr uint32_t gen_dilate(uint32_t a, uint32_t offset)
{
    for (int ii = 0; ii < 4; ++ii)
        a = (a | (a << (4 * sizeof(uint32_t) >> ii))) & dilate_masks_32[ii];

    return a << offset;
}

static constexpr uint32_t gen_contract(uint32_t a, uint32_t offset)
{
    a = a >> offset;
    a = a & dilate_masks_32[3];
    for (int ii = 0; ii < 3; ++ii)
        a = (a | (a >> (1 << (ii + 1)))) & dilate_masks_32[2 - ii];

    return a;
}

template <typename T, std::size_t SIZE>
static constexpr std::array<T, SIZE> make_dilation_LUT(uint32_t offset)
{
    std::array<T, SIZE> result{};
    for (T ii = 0; ii < T(SIZE); ++ii)
        result[ii] = T(gen_dilate(uint32_t(ii), offset));

    return result;
}

template <typename T, std::size_t SIZE>
static constexpr std::array<T, SIZE> make_contraction_LUT(uint32_t offset)
{
    std::array<T, SIZE> result{};
    for (std::size_t ii = 0; ii < SIZE; ++ii)
        result[ii] = T(gen_contract(uint32_t(ii), offset));

    return result;
}

class LUT
{
public:
    static constexpr std::array<uint32_t, 256> dilation_x = make_dilation_LUT<uint32_t, 256>(0);
    static constexpr std::array<uint32_t, 256> dilation_y = make_dilation_LUT<uint32_t, 256>(1);
    static constexpr std::array<uint32_t, 256> dilation_z = make_dilation_LUT<uint32_t, 256>(2);
    static constexpr std::array<uint8_t, 512> contraction_x = make_contraction_LUT<uint8_t, 512>(0);
    static constexpr std::array<uint8_t, 512> contraction_y = make_contraction_LUT<uint8_t, 512>(1);
    static constexpr std::array<uint8_t, 512> contraction_z = make_contraction_LUT<uint8_t, 512>(2);

    // HELPER METHODE for LUT decoding
    template <typename keyT, typename coordT>
    static inline coordT decode(const keyT m, const uint8_t* LUT, const unsigned int startshift)
    {
        keyT a = 0;
        unsigned int loops = (sizeof(keyT) <= 4) ? 4 : 7; // ceil for 32bit, floor for 64bit
        for (unsigned int ii = 0; ii < loops; ++ii)
            a |= keyT(LUT[(m >> ((ii * 9) + startshift)) & mask_9<keyT>()] << keyT(3 * ii));

        return static_cast<coordT>(a);
    }
};
} // namespace detail

// ENCODE 3D Morton code : Pre-Shifted LookUpTable
template <typename keyT, typename coordT>
inline keyT encode(const coordT x, const coordT y, const coordT z)
{
    keyT answer = 0;
    for (coordT ii = sizeof(coordT); ii > 0; --ii)
    {
        coordT shift = (ii - 1) * 8;
        answer = answer << 24 | (detail::LUT::dilation_z[(z >> shift) & coordT(detail::mask_8<keyT>())] |
                                 detail::LUT::dilation_y[(y >> shift) & coordT(detail::mask_8<keyT>())] |
                                 detail::LUT::dilation_x[(x >> shift) & coordT(detail::mask_8<keyT>())]);
    }
    return answer;
}

// DECODE 3D Morton code : Shifted LUT
template <typename keyT, typename coordT>
inline std::tuple<coordT, coordT, coordT> decode(const keyT m)
{
    return {detail::LUT::decode<keyT, coordT>(m, &detail::LUT::contraction_x[0], 0),
            detail::LUT::decode<keyT, coordT>(m, &detail::LUT::contraction_y[0], 0),
            detail::LUT::decode<keyT, coordT>(m, &detail::LUT::contraction_z[0], 0)};
}

} // namespace impl3d
} // namespace kb::morton