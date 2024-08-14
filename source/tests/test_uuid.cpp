#include "kibble/random/uuid.h"

#include <catch2/catch_all.hpp>

/**
 * @brief Unit tests for UUIDv4
 *
 * Adapted from:
 * https://github.com/crashoz/uuid_v4/blob/master/tests/uuid_v4_test.cpp
 *
 * Modifications are:
 * - Using Catch2 instead of GTest
 *
 */

using namespace kb;

void pb(const std::string& s)
{
    for (size_t i = 0; i < 16; i++)
    {
        printf("%02hhx", s[i]);
    }
    printf("\n");
}

bool isBinaryLE(uint64_t x, uint64_t y, const std::string& bytes)
{
    for (size_t i = 0; i < 8; i++)
    {
        if (static_cast<unsigned char>(bytes[i]) != ((x >> i * 8) & 0xFF))
        {
            return false;
        }
    }
    for (size_t i = 8; i < 16; i++)
    {
        if (static_cast<unsigned char>(bytes[i]) != ((y >> (i - 8) * 8) & 0xFF))
        {
            return false;
        }
    }
    return true;
}

TEST_CASE("SerializeUUIDinLE")
{
    uint64_t x = 0x0012003400560078ull;
    uint64_t y = 0x0012003400560078ull;
    UUIDv4::UUID uuid(x, y);
    std::string bytes = uuid.bytes();
    REQUIRE(isBinaryLE(x, y, bytes));
}

TEST_CASE("PrettyPrints")
{
    uint8_t bytes[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    UUIDv4::UUID uuid(bytes);
    std::string pretty = uuid.str();
    REQUIRE(pretty == "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST_CASE("UnserializeFromLE")
{
    std::string bytes = {0x78, 0x00, 0x56, 0x00, 0x34, 0x00, 0x12, 0x00,
                         0x78, 0x00, 0x56, 0x00, 0x34, 0x00, 0x12, 0x00};
    UUIDv4::UUID uuid(bytes);
    REQUIRE(uuid.str() == "78005600-3400-1200-7800-560034001200");
}

TEST_CASE("ParsePretty")
{
    std::string bytes = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                         0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    UUIDv4::UUID uuid = UUIDv4::UUID::from_str_factory("00010203-0405-0607-0809-0a0b0c0d0e0f");
    REQUIRE(uuid.bytes() == bytes);
}

TEST_CASE("StreamOperators")
{
    UUIDv4::UUID uuid;
    std::string pretty = "00120034-0056-0078-0012-003400560078";
    std::istringstream in(pretty);
    std::ostringstream out;

    in >> uuid;
    out << uuid;

    REQUIRE(out.str() == pretty);
}

TEST_CASE("Comparisons")
{
    UUIDv4::UUID uuid = UUIDv4::UUID::from_str_factory("00120034-0056-0078-0012-003400560078");
    UUIDv4::UUID uuid2 = UUIDv4::UUID(uuid);

    REQUIRE(uuid == uuid2);
    REQUIRE_FALSE(uuid < uuid2);

    UUIDv4::UUID uuid3 = UUIDv4::UUID::from_str_factory("f0120034-0056-0078-0012-003400560078");
    REQUIRE(uuid < uuid3);

    UUIDv4::UUID uuid4 = UUIDv4::UUID::from_str_factory("00020034-0056-0078-0012-003400560078");
    REQUIRE_FALSE(uuid < uuid4);
    REQUIRE(uuid > uuid4);

    UUIDv4::UUID uuid5 = UUIDv4::UUID::from_str_factory("fc120034-0056-0078-0012-003400560078");
    REQUIRE(uuid < uuid5);
    REQUIRE_FALSE(uuid > uuid5);
}