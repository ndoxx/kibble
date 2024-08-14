#include "kibble/math/morton.h"

#if defined(__BMI2__) || (defined(__AVX2__) && defined(_MSC_VER))
#include "kibble/math/impl/morton_bmi.h"
#else
#include "kibble/math/impl/morton_lut.h"
#endif

namespace kb::morton
{

template <>
uint32_t encode(const uint32_t x, const uint32_t y)
{
    return impl2d::encode<uint32_t, uint32_t>(x, y);
}

template <>
uint32_t encode(const uint32_t x, const uint32_t y, const uint32_t z)
{
    return impl3d::encode<uint32_t, uint32_t>(x, y, z);
}

template <>
uint64_t encode(const uint64_t x, const uint64_t y)
{
    return impl2d::encode<uint64_t, uint64_t>(x, y);
}

template <>
uint64_t encode(const uint64_t x, const uint64_t y, const uint64_t z)
{
    return impl3d::encode<uint64_t, uint64_t>(x, y, z);
}

template <>
std::tuple<uint32_t, uint32_t> decode_2d(const uint32_t key)
{
    return impl2d::decode<uint32_t, uint32_t>(key);
}

template <>
std::tuple<uint64_t, uint64_t> decode_2d(const uint64_t key)
{
    return impl2d::decode<uint64_t, uint64_t>(key);
}

template <>
std::tuple<uint32_t, uint32_t, uint32_t> decode_3d(const uint32_t key)
{
    return impl3d::decode<uint32_t, uint32_t>(key);
}

template <>
std::tuple<uint64_t, uint64_t, uint64_t> decode_3d(const uint64_t key)
{
    return impl3d::decode<uint64_t, uint64_t>(key);
}

} // namespace kb::morton