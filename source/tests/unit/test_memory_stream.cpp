#include "kibble/filesystem/stream/memory_stream.h"

#include <catch2/catch_all.hpp>

TEST_CASE("MemoryBuffer construction", "[MemoryBuffer]")
{
    SECTION("Valid construction")
    {
        uint8_t buffer[100];
        REQUIRE_NOTHROW(kb::MemoryBuffer(buffer, 100));
    }

    SECTION("Null buffer")
    {
        REQUIRE_THROWS_AS(kb::MemoryBuffer(nullptr, 100), std::runtime_error);
    }

    SECTION("Zero size")
    {
        uint8_t buffer[100];
        REQUIRE_THROWS_AS(kb::MemoryBuffer(buffer, 0), std::runtime_error);
    }
}

TEST_CASE("MemoryInputStream functionality", "[MemoryInputStream]")
{
    uint8_t buffer[100];
    for (size_t ii = 0; ii < 100; ++ii)
    {
        buffer[ii] = static_cast<uint8_t>(ii);
    }
    kb::InputMemoryStream stream(buffer, 100);

    SECTION("Reading data")
    {
        char read_buf[10];
        stream.read(read_buf, 10);
        REQUIRE(stream.gcount() == 10);
        for (size_t ii = 0; ii < 10; ++ii)
        {
            REQUIRE(static_cast<size_t>(read_buf[ii]) == ii);
        }
    }

    SECTION("Seeking and telling")
    {
        REQUIRE(stream.tellg() == 0);
        stream.seekg(50);
        REQUIRE(stream.tellg() == 50);
        char read_buf[10];
        stream.read(read_buf, 10);
        REQUIRE(static_cast<uint8_t>(read_buf[0]) == 50);
    }

    SECTION("Reading past end")
    {
        stream.seekg(95);
        char read_buf[10];
        stream.read(read_buf, 10);
        REQUIRE(stream.gcount() == 5);
        REQUIRE(stream.eof());
        REQUIRE(stream.fail());
    }
}

TEST_CASE("MemoryOutputStream functionality", "[MemoryOutputStream]")
{
    uint8_t buffer[100] = {0};
    kb::OutputMemoryStream stream(buffer, 100);

    SECTION("Writing data")
    {
        const char* data = "Hello, World!";
        stream.write(data, 13);
        REQUIRE(stream.tellp() == 13);
        REQUIRE(std::memcmp(buffer, data, 13) == 0);
    }

    SECTION("Seeking and telling")
    {
        REQUIRE(stream.tellp() == 0);
        stream.seekp(50);
        REQUIRE(stream.tellp() == 50);
        stream.put('A');
        REQUIRE(buffer[50] == 'A');
    }

    SECTION("Writing past end")
    {
        stream.seekp(95);
        const char* data = "Hello, World!";
        stream.write(data, 13);
        REQUIRE(stream.tellp() == -1);
        REQUIRE(stream.fail());
        REQUIRE(std::memcmp(buffer + 95, data, 5) == 0);
    }
}

TEST_CASE("MemoryBuffer seeking", "[MemoryBuffer]")
{
    uint8_t buffer[100];
    kb::MemoryBuffer mem_buffer(buffer, 100);

    SECTION("Seeking from beginning")
    {
        auto pos = mem_buffer.pubseekoff(50, std::ios_base::beg, std::ios_base::in | std::ios_base::out);
        REQUIRE(pos == 50);
    }

    SECTION("Seeking from current position")
    {
        mem_buffer.pubseekoff(25, std::ios_base::beg, std::ios_base::in | std::ios_base::out);
        auto pos = mem_buffer.pubseekoff(25, std::ios_base::cur, std::ios_base::in | std::ios_base::out);
        REQUIRE(pos == 50);
    }

    SECTION("Seeking from end")
    {
        auto pos = mem_buffer.pubseekoff(-50, std::ios_base::end, std::ios_base::in | std::ios_base::out);
        REQUIRE(pos == 50);
    }

    SECTION("Seeking past end")
    {
        auto pos = mem_buffer.pubseekoff(101, std::ios_base::beg, std::ios_base::in | std::ios_base::out);
        REQUIRE(pos == -1);
    }

    SECTION("Seeking before beginning")
    {
        auto pos = mem_buffer.pubseekoff(-1, std::ios_base::beg, std::ios_base::in | std::ios_base::out);
        REQUIRE(pos == -1);
    }
}