#include "morton.h"

#if defined(__BMI2__) || (defined(__AVX2__) && defined(_MSC_VER))
#include "impl/morton_bmi.h"
#else
#include "impl/morton_lut.h"
#endif

namespace kb::morton
{

uint32_t encode_32(const uint32_t x, const uint32_t y)
{
    return impl2d::encode<uint32_t, uint32_t>(x, y);
}

uint64_t encode_64(const uint64_t x, const uint64_t y)
{
    return impl2d::encode<uint64_t, uint64_t>(x, y);
}

uint32_t encode_32(const uint32_t x, const uint32_t y, const uint32_t z)
{
    return impl3d::encode<uint32_t, uint32_t>(x, y, z);
}

uint64_t encode_64(const uint64_t x, const uint64_t y, const uint64_t z)
{
    return impl3d::encode<uint64_t, uint64_t>(x, y, z);
}

std::tuple<uint32_t, uint32_t> decode_2d_32(const uint32_t key)
{
    return impl2d::decode<uint32_t, uint32_t>(key);
}

std::tuple<uint64_t, uint64_t> decode_2d_64(const uint64_t key)
{
    return impl2d::decode<uint64_t, uint64_t>(key);
}

std::tuple<uint32_t, uint32_t, uint32_t> decode_3d_32(const uint32_t key)
{
    return impl3d::decode<uint32_t, uint32_t>(key);
}

std::tuple<uint64_t, uint64_t, uint64_t> decode_3d_64(const uint64_t key)
{
    return impl3d::decode<uint64_t, uint64_t>(key);
}

} // namespace kb::morton