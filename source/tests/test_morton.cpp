#include "algorithm/msb_search.h"
#include "math/morton.h"
#include <catch2/catch_all.hpp>
#include <numeric>
#include <random>

using namespace kb;

/*
    Naive readable implementations of Morton encoding / decoding
    These functions perform bit (de-)interleaving in an iterative fashion
*/

uint32_t encode_naive_32(uint32_t x, uint32_t y)
{
    uint32_t key = 0;

    for (size_t ii = 0; ii < 16; ++ii)
    {
        uint32_t x_bit = (x & (1u << ii)) >> ii;
        uint32_t y_bit = (y & (1u << ii)) >> ii;

        key |= (x_bit << (2 * ii + 0)) | (y_bit << (2 * ii + 1));
    }

    return key;
}

uint64_t encode_naive_64(uint64_t x, uint64_t y)
{
    uint64_t key = 0;

    for (size_t ii = 0; ii < 32; ++ii)
    {
        uint64_t x_bit = (x & (1ul << ii)) >> ii;
        uint64_t y_bit = (y & (1ul << ii)) >> ii;

        key |= (x_bit << (2 * ii + 0)) | (y_bit << (2 * ii + 1));
    }

    return key;
}

uint32_t encode_naive_32(uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t key = 0;

    for (size_t ii = 0; ii < 10; ++ii)
    {
        uint32_t x_bit = (x & (1u << ii)) >> ii;
        uint32_t y_bit = (y & (1u << ii)) >> ii;
        uint32_t z_bit = (z & (1u << ii)) >> ii;

        key |= (x_bit << (3 * ii + 0)) | (y_bit << (3 * ii + 1)) | (z_bit << (3 * ii + 2));
    }

    return key;
}

uint64_t encode_naive_64(uint64_t x, uint64_t y, uint64_t z)
{
    uint64_t key = 0;

    for (size_t ii = 0; ii < 21; ++ii)
    {
        uint64_t x_bit = (x & (1ul << ii)) >> ii;
        uint64_t y_bit = (y & (1ul << ii)) >> ii;
        uint64_t z_bit = (z & (1ul << ii)) >> ii;

        key |= (x_bit << (3 * ii + 0)) | (y_bit << (3 * ii + 1)) | (z_bit << (3 * ii + 2));
    }

    return key;
}

std::tuple<uint32_t, uint32_t> decode_2d_naive_32(uint32_t key)
{
    uint32_t x = 0, y = 0;

    for (size_t ii = 0; ii < 16; ++ii)
    {
        uint32_t x_bit = (key & (1u << (2 * ii + 0))) >> (2 * ii + 0);
        uint32_t y_bit = (key & (1u << (2 * ii + 1))) >> (2 * ii + 1);

        x |= x_bit << ii;
        y |= y_bit << ii;
    }

    return {x, y};
}

std::tuple<uint64_t, uint64_t> decode_2d_naive_64(uint64_t key)
{
    uint64_t x = 0, y = 0;

    for (size_t ii = 0; ii < 32; ++ii)
    {
        uint64_t x_bit = (key & (1ul << (2 * ii + 0))) >> (2 * ii + 0);
        uint64_t y_bit = (key & (1ul << (2 * ii + 1))) >> (2 * ii + 1);

        x |= x_bit << ii;
        y |= y_bit << ii;
    }

    return {x, y};
}

std::tuple<uint32_t, uint32_t, uint32_t> decode_3d_naive_32(uint32_t key)
{
    uint32_t x = 0, y = 0, z = 0;

    for (size_t ii = 0; ii < 10; ++ii)
    {
        uint32_t x_bit = (key & (1u << (3 * ii + 0))) >> (3 * ii + 0);
        uint32_t y_bit = (key & (1u << (3 * ii + 1))) >> (3 * ii + 1);
        uint32_t z_bit = (key & (1u << (3 * ii + 2))) >> (3 * ii + 2);

        x |= x_bit << ii;
        y |= y_bit << ii;
        z |= z_bit << ii;
    }

    return {x, y, z};
}

std::tuple<uint64_t, uint64_t, uint64_t> decode_3d_naive_64(uint64_t key)
{
    uint64_t x = 0, y = 0, z = 0;

    for (size_t ii = 0; ii < 21; ++ii)
    {
        uint64_t x_bit = (key & (1ul << (3 * ii + 0))) >> (3 * ii + 0);
        uint64_t y_bit = (key & (1ul << (3 * ii + 1))) >> (3 * ii + 1);
        uint64_t z_bit = (key & (1ul << (3 * ii + 2))) >> (3 * ii + 2);

        x |= x_bit << ii;
        y |= y_bit << ii;
        z |= z_bit << ii;
    }

    return {x, y, z};
}

TEST_CASE("Encoding, 2D, 32b")
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint32_t> dist(0, 1024);

    bool pass = true;
    for (size_t ii = 0; ii < 1000; ++ii)
    {
        uint32_t x = dist(rng);
        uint32_t y = dist(rng);
        uint32_t m = encode_naive_32(x, y);
        uint32_t m_t = kb::morton::encode(x, y);

        pass &= (m == m_t);
    }

    REQUIRE(pass);
}

TEST_CASE("Encoding, 2D, 64b")
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 1024);

    bool pass = true;
    for (size_t ii = 0; ii < 1000; ++ii)
    {
        uint64_t x = dist(rng);
        uint64_t y = dist(rng);
        uint64_t m = encode_naive_64(x, y);
        uint64_t m_t = kb::morton::encode(x, y);

        pass &= (m == m_t);
    }

    REQUIRE(pass);
}

TEST_CASE("Encoding, 3D, 32b")
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint32_t> dist(0, 128);

    bool pass = true;
    for (size_t ii = 0; ii < 1000; ++ii)
    {
        uint32_t x = dist(rng);
        uint32_t y = dist(rng);
        uint32_t z = dist(rng);
        uint32_t m = encode_naive_32(x, y, z);
        uint32_t m_t = kb::morton::encode(x, y, z);

        pass &= (m == m_t);
    }

    REQUIRE(pass);
}

TEST_CASE("Encoding, 3D, 64b")
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 128);

    bool pass = true;
    for (size_t ii = 0; ii < 1000; ++ii)
    {
        uint64_t x = dist(rng);
        uint64_t y = dist(rng);
        uint64_t z = dist(rng);
        uint64_t m = encode_naive_64(x, y, z);
        uint64_t m_t = kb::morton::encode(x, y, z);

        pass &= (m == m_t);
    }

    REQUIRE(pass);
}

TEST_CASE("Decoding, 2D, 32b")
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint32_t> dist(0, 32768);

    bool pass = true;
    for (size_t ii = 0; ii < 1000; ++ii)
    {
        uint32_t key = dist(rng);
        auto [x, y] = decode_2d_naive_32(key);
        auto [x_t, y_t] = kb::morton::decode_2d(key);

        pass &= (x == x_t) && (y == y_t);
    }

    REQUIRE(pass);
}

TEST_CASE("Decoding, 2D, 64b")
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 32768);

    bool pass = true;
    for (size_t ii = 0; ii < 1000; ++ii)
    {
        uint64_t key = dist(rng);
        auto [x, y] = decode_2d_naive_64(key);
        auto [x_t, y_t] = kb::morton::decode_2d(key);

        pass &= (x == x_t) && (y == y_t);
    }

    REQUIRE(pass);
}

TEST_CASE("Decoding, 3D, 32b")
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint32_t> dist(0, 32768);

    bool pass = true;
    for (size_t ii = 0; ii < 1000; ++ii)
    {
        uint32_t key = dist(rng);
        auto [x, y, z] = decode_3d_naive_32(key);
        auto [x_t, y_t, z_t] = kb::morton::decode_3d(key);

        pass &= (x == x_t) && (y == y_t) && (z == z_t);
    }

    REQUIRE(pass);
}

TEST_CASE("Decoding, 3D, 64b")
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(0, 32768);

    bool pass = true;
    for (size_t ii = 0; ii < 1000; ++ii)
    {
        uint64_t key = dist(rng);
        auto [x, y, z] = decode_3d_naive_64(key);
        auto [x_t, y_t, z_t] = kb::morton::decode_3d(key);

        pass &= (x == x_t) && (y == y_t) && (z == z_t);
    }

    REQUIRE(pass);
}

TEST_CASE("MSB search 32b", "[msb]")
{
    std::array<size_t, 32> int_seq;
    std::iota(std::begin(int_seq), std::end(int_seq), 0);

    std::array<size_t, 32> ans;
    std::generate(std::begin(ans), std::end(ans), [ii = 0]() mutable { return kb::msb_search<uint32_t>(1u << ii++); });

    REQUIRE(ans == int_seq);
}

TEST_CASE("MSB search 64b", "[msb]")
{
    std::array<size_t, 64> int_seq;
    std::iota(std::begin(int_seq), std::end(int_seq), 0);

    std::array<size_t, 64> ans;
    std::generate(std::begin(ans), std::end(ans), [ii = 0]() mutable { return kb::msb_search<uint64_t>(1ul << ii++); });

    REQUIRE(ans == int_seq);
}