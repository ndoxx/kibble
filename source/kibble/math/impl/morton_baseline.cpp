#include <array>
#include <cstdint>
#include <tuple>

namespace kb::morton::impl::baseline
{

template <typename T>
static constexpr T mask_8()
{
    return T(0x000000FF);
}
template <typename T>
static constexpr T mask_9()
{
    return T(0x000001FF);
}

// Inspired by the artricle: Integer Dilation and Contraction for Quadtrees and Octrees (Leo Stocco & Gunther Schrack)
static constexpr uint32_t dilate_masks_32_2D[6] = {0xFFFFFFFF, 0x0000FFFF, 0x00FF00FF,
                                                   0x0F0F0F0F, 0x33333333, 0x55555555};
static constexpr uint32_t dilate_masks_32_3D[4] = {0x030000FF, 0x0300F00F, 0x030C30C3, 0x09249249};

static constexpr uint32_t gen_dilate_2D(uint32_t a, uint32_t offset)
{
    for (int ii = 1; ii < 6; ++ii)
    {
        a = (a | (a << (4 * sizeof(uint32_t) >> (ii - 1)))) & dilate_masks_32_2D[ii];
    }

    return a << offset;
}

static constexpr uint32_t gen_contract_2D(uint32_t a, uint32_t offset)
{
    a = a >> offset;
    a = a & dilate_masks_32_2D[5];
    for (int ii = 0; ii < 5; ++ii)
    {
        a = (a | (a >> (1 << ii))) & dilate_masks_32_2D[4 - ii];
    }

    return a;
}

static constexpr uint32_t gen_dilate_3D(uint32_t a, uint32_t offset)
{
    for (int ii = 0; ii < 4; ++ii)
    {
        a = (a | (a << (4 * sizeof(uint32_t) >> ii))) & dilate_masks_32_3D[ii];
    }

    return a << offset;
}

static constexpr uint32_t gen_contract_3D(uint32_t a, uint32_t offset)
{
    a = a >> offset;
    a = a & dilate_masks_32_3D[3];
    for (int ii = 0; ii < 3; ++ii)
    {
        a = (a | (a >> (1 << (ii + 1)))) & dilate_masks_32_3D[2 - ii];
    }

    return a;
}

template <typename T, std::size_t SIZE>
static constexpr std::array<T, SIZE> make_dilation_LUT_2D(uint32_t offset)
{
    std::array<T, SIZE> result{};
    for (T ii = 0; ii < T(SIZE); ++ii)
    {
        result[ii] = T(gen_dilate_2D(uint32_t(ii), offset));
    }

    return result;
}

template <typename T, std::size_t SIZE>
static constexpr std::array<T, SIZE> make_contraction_LUT_2D(uint32_t offset)
{
    std::array<T, SIZE> result{};
    for (std::size_t ii = 0; ii < SIZE; ++ii)
    {
        result[ii] = T(gen_contract_2D(uint32_t(ii), offset));
    }

    return result;
}

template <typename T, std::size_t SIZE>
static constexpr std::array<T, SIZE> make_dilation_LUT_3D(uint32_t offset)
{
    std::array<T, SIZE> result{};
    for (T ii = 0; ii < T(SIZE); ++ii)
    {
        result[ii] = T(gen_dilate_3D(uint32_t(ii), offset));
    }

    return result;
}

template <typename T, std::size_t SIZE>
static constexpr std::array<T, SIZE> make_contraction_LUT_3D(uint32_t offset)
{
    std::array<T, SIZE> result{};
    for (std::size_t ii = 0; ii < SIZE; ++ii)
    {
        result[ii] = T(gen_contract_3D(uint32_t(ii), offset));
    }

    return result;
}

class LUT2D
{
public:
    static constexpr std::array<uint16_t, 256> dilation_x = make_dilation_LUT_2D<uint16_t, 256>(0);
    static constexpr std::array<uint16_t, 256> dilation_y = make_dilation_LUT_2D<uint16_t, 256>(1);
    static constexpr std::array<uint8_t, 256> contraction_x = make_contraction_LUT_2D<uint8_t, 256>(0);
    static constexpr std::array<uint8_t, 256> contraction_y = make_contraction_LUT_2D<uint8_t, 256>(1);

    // HELPER METHODE for LUT decoding
    template <typename keyT, typename coordT>
    static inline coordT decode(const keyT m, const uint8_t* LUT, const unsigned int startshift)
    {
        keyT a = 0;
        unsigned int loops = sizeof(keyT);
        for (unsigned int ii = 0; ii < loops; ++ii)
        {
            a |= keyT(LUT[(m >> ((ii * 8) + startshift)) & mask_8<keyT>()] << (4 * ii));
        }

        return static_cast<coordT>(a);
    }
};

class LUT3D
{
public:
    static constexpr std::array<uint32_t, 256> dilation_x = make_dilation_LUT_3D<uint32_t, 256>(0);
    static constexpr std::array<uint32_t, 256> dilation_y = make_dilation_LUT_3D<uint32_t, 256>(1);
    static constexpr std::array<uint32_t, 256> dilation_z = make_dilation_LUT_3D<uint32_t, 256>(2);
    static constexpr std::array<uint8_t, 512> contraction_x = make_contraction_LUT_3D<uint8_t, 512>(0);
    static constexpr std::array<uint8_t, 512> contraction_y = make_contraction_LUT_3D<uint8_t, 512>(1);
    static constexpr std::array<uint8_t, 512> contraction_z = make_contraction_LUT_3D<uint8_t, 512>(2);

    // HELPER METHODE for LUT decoding
    template <typename keyT, typename coordT>
    static inline coordT decode(const keyT m, const uint8_t* LUT, const unsigned int startshift)
    {
        keyT a = 0;
        unsigned int loops = (sizeof(keyT) <= 4) ? 4 : 7; // ceil for 32bit, floor for 64bit
        for (unsigned int ii = 0; ii < loops; ++ii)
        {
            a |= keyT(LUT[(m >> ((ii * 9) + startshift)) & mask_9<keyT>()] << keyT(3 * ii));
        }

        return static_cast<coordT>(a);
    }
};

// ENCODE 2D Morton code: Pre-shifted LookUpTable
uint32_t encode_32_2d(const uint32_t x, const uint32_t y)
{
    uint32_t answer = 0;
    for (uint32_t ii = sizeof(uint32_t); ii > 0; --ii)
    {
        uint32_t shift = (ii - 1) * 8;
        answer = answer << 16 | LUT2D::dilation_y[(y >> shift) & uint32_t(mask_8<uint32_t>())] |
                 LUT2D::dilation_x[(x >> shift) & uint32_t(mask_8<uint32_t>())];
    }
    return answer;
}
uint64_t encode_64_2d(const uint64_t x, const uint64_t y)
{
    uint64_t answer = 0;
    for (uint64_t ii = sizeof(uint64_t); ii > 0; --ii)
    {
        uint64_t shift = (ii - 1) * 8;
        answer = answer << 16 | LUT2D::dilation_y[(y >> shift) & uint64_t(mask_8<uint64_t>())] |
                 LUT2D::dilation_x[(x >> shift) & uint64_t(mask_8<uint64_t>())];
    }
    return answer;
}

// ENCODE 3D Morton code : Pre-Shifted LookUpTable
uint32_t encode_32_3d(const uint32_t x, const uint32_t y, const uint32_t z)
{
    uint32_t answer = 0;
    for (uint32_t ii = sizeof(uint32_t); ii > 0; --ii)
    {
        uint32_t shift = (ii - 1) * 8;
        answer = answer << 24 | (LUT3D::dilation_z[(z >> shift) & uint32_t(mask_8<uint32_t>())] |
                                 LUT3D::dilation_y[(y >> shift) & uint32_t(mask_8<uint32_t>())] |
                                 LUT3D::dilation_x[(x >> shift) & uint32_t(mask_8<uint32_t>())]);
    }
    return answer;
}
uint64_t encode_64_3d(const uint64_t x, const uint64_t y, const uint64_t z)
{
    uint64_t answer = 0;
    for (uint64_t ii = sizeof(uint64_t); ii > 0; --ii)
    {
        uint64_t shift = (ii - 1) * 8;
        answer = answer << 24 | (LUT3D::dilation_z[(z >> shift) & uint64_t(mask_8<uint64_t>())] |
                                 LUT3D::dilation_y[(y >> shift) & uint64_t(mask_8<uint64_t>())] |
                                 LUT3D::dilation_x[(x >> shift) & uint64_t(mask_8<uint64_t>())]);
    }
    return answer;
}

// DECODE 2D Morton code : Shifted LUT
std::tuple<uint32_t, uint32_t> decode_32_2d(const uint32_t m)
{
    return {LUT2D::decode<uint32_t, uint32_t>(m, &LUT2D::contraction_x[0], 0),
            LUT2D::decode<uint32_t, uint32_t>(m, &LUT2D::contraction_y[0], 0)};
}
std::tuple<uint64_t, uint64_t> decode_64_2d(const uint64_t m)
{
    return {LUT2D::decode<uint64_t, uint64_t>(m, &LUT2D::contraction_x[0], 0),
            LUT2D::decode<uint64_t, uint64_t>(m, &LUT2D::contraction_y[0], 0)};
}

// DECODE 3D Morton code : Shifted LUT
std::tuple<uint32_t, uint32_t, uint32_t> decode_32_3d(const uint32_t m)
{
    return {LUT3D::decode<uint32_t, uint32_t>(m, &LUT3D::contraction_x[0], 0),
            LUT3D::decode<uint32_t, uint32_t>(m, &LUT3D::contraction_y[0], 0),
            LUT3D::decode<uint32_t, uint32_t>(m, &LUT3D::contraction_z[0], 0)};
}
std::tuple<uint64_t, uint64_t, uint64_t> decode_64_3d(const uint64_t m)
{
    return {LUT3D::decode<uint64_t, uint64_t>(m, &LUT3D::contraction_x[0], 0),
            LUT3D::decode<uint64_t, uint64_t>(m, &LUT3D::contraction_y[0], 0),
            LUT3D::decode<uint64_t, uint64_t>(m, &LUT3D::contraction_z[0], 0)};
}

} // namespace kb::morton::impl::baseline