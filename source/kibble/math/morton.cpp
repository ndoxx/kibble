#include "kibble/math/morton.h"
#include "kibble/platform/arch.h"

namespace kb::morton
{

// Forward declarations
namespace impl::baseline
{
// Implemented in impl/morton_lut.cpp
uint32_t encode_32_2d(const uint32_t x, const uint32_t y);
uint64_t encode_64_2d(const uint64_t x, const uint64_t y);
uint32_t encode_32_3d(const uint32_t x, const uint32_t y, const uint32_t z);
uint64_t encode_64_3d(const uint64_t x, const uint64_t y, const uint64_t z);
std::tuple<uint32_t, uint32_t> decode_32_2d(const uint32_t m);
std::tuple<uint64_t, uint64_t> decode_64_2d(const uint64_t m);
std::tuple<uint32_t, uint32_t, uint32_t> decode_32_3d(const uint32_t m);
std::tuple<uint64_t, uint64_t, uint64_t> decode_64_3d(const uint64_t m);
} // namespace impl::baseline

namespace impl::bmi2
{
// Implemented in impl/morton_bmi2.cpp
uint32_t encode_32_2d(const uint32_t x, const uint32_t y);
uint64_t encode_64_2d(const uint64_t x, const uint64_t y);
uint32_t encode_32_3d(const uint32_t x, const uint32_t y, const uint32_t z);
uint64_t encode_64_3d(const uint64_t x, const uint64_t y, const uint64_t z);
std::tuple<uint32_t, uint32_t> decode_32_2d(const uint32_t m);
std::tuple<uint64_t, uint64_t> decode_64_2d(const uint64_t m);
std::tuple<uint32_t, uint32_t, uint32_t> decode_32_3d(const uint32_t m);
std::tuple<uint64_t, uint64_t, uint64_t> decode_64_3d(const uint64_t m);
} // namespace impl::bmi2

template <>
uint32_t encode(const uint32_t x, const uint32_t y)
{
    static auto dispatch_func = []() -> uint32_t (*)(const uint32_t, const uint32_t) {
        return CPUInfo::has_ISA_BMI2() ? impl::bmi2::encode_32_2d : impl::baseline::encode_32_2d;
    }();

    return dispatch_func(x, y);
}
template <>
uint64_t encode(const uint64_t x, const uint64_t y)
{
    static auto dispatch_func = []() -> uint64_t (*)(const uint64_t, const uint64_t) {
        return CPUInfo::has_ISA_BMI2() ? impl::bmi2::encode_64_2d : impl::baseline::encode_64_2d;
    }();

    return dispatch_func(x, y);
}

template <>
uint32_t encode(const uint32_t x, const uint32_t y, const uint32_t z)
{
    static auto dispatch_func = []() -> uint32_t (*)(const uint32_t, const uint32_t, const uint32_t) {
        return CPUInfo::has_ISA_BMI2() ? impl::bmi2::encode_32_3d : impl::baseline::encode_32_3d;
    }();

    return dispatch_func(x, y, z);
}
template <>
uint64_t encode(const uint64_t x, const uint64_t y, const uint64_t z)
{
    static auto dispatch_func = []() -> uint64_t (*)(const uint64_t, const uint64_t, const uint64_t) {
        return CPUInfo::has_ISA_BMI2() ? impl::bmi2::encode_64_3d : impl::baseline::encode_64_3d;
    }();

    return dispatch_func(x, y, z);
}

template <>
std::tuple<uint32_t, uint32_t> decode_2d(const uint32_t key)
{
    static auto dispatch_func = []() -> std::tuple<uint32_t, uint32_t> (*)(const uint32_t) {
        return CPUInfo::has_ISA_BMI2() ? impl::bmi2::decode_32_2d : impl::baseline::decode_32_2d;
    }();

    return dispatch_func(key);
}
template <>
std::tuple<uint64_t, uint64_t> decode_2d(const uint64_t key)
{
    static auto dispatch_func = []() -> std::tuple<uint64_t, uint64_t> (*)(const uint64_t) {
        return CPUInfo::has_ISA_BMI2() ? impl::bmi2::decode_64_2d : impl::baseline::decode_64_2d;
    }();

    return dispatch_func(key);
}

template <>
std::tuple<uint32_t, uint32_t, uint32_t> decode_3d(const uint32_t key)
{
    static auto dispatch_func = []() -> std::tuple<uint32_t, uint32_t, uint32_t> (*)(const uint32_t) {
        return CPUInfo::has_ISA_BMI2() ? impl::bmi2::decode_32_3d : impl::baseline::decode_32_3d;
    }();

    return dispatch_func(key);
}

template <>
std::tuple<uint64_t, uint64_t, uint64_t> decode_3d(const uint64_t key)
{
    static auto dispatch_func = []() -> std::tuple<uint64_t, uint64_t, uint64_t> (*)(const uint64_t) {
        return CPUInfo::has_ISA_BMI2() ? impl::bmi2::decode_64_3d : impl::baseline::decode_64_3d;
    }();

    return dispatch_func(key);
}

} // namespace kb::morton