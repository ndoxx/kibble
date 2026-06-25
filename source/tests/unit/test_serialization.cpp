#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "kibble/filesystem/serialization/std_archiver.h"
#include "kibble/filesystem/serialization/stream_serializer.h"
#include "kibble/filesystem/stream/memory_stream.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace kb;

// Custom trivial test class
struct TrivialTestStruct
{
    int32_t a;
    float b;
    bool c;

    bool operator==(const TrivialTestStruct& other) const
    {
        return a == other.a && b == other.b && c == other.c;
    }
};

// Make it trivially serializable
namespace kb
{
template <>
struct is_trivially_serializable<TrivialTestStruct> : std::true_type
{
};
} // namespace kb

// Custom non-trivial test class with Archiver specialization
struct CustomTestStruct
{
    int32_t id;
    std::string name;
    std::vector<float> values;

    bool operator==(const CustomTestStruct& other) const
    {
        return id == other.id && name == other.name && values == other.values;
    }
};

namespace kb
{
template <>
struct Archiver<CustomTestStruct>
{
    static bool write(const CustomTestStruct& object, StreamSerializer& ser)
    {
        return ser.write(object.id) && ser.write(object.name) && ser.write(object.values);
    }

    static bool read(CustomTestStruct& object, StreamDeserializer& des)
    {
        return des.read(object.id) && des.read(object.name) && des.read(object.values);
    }
};
} // namespace kb

// Versioned test class
struct VersionedTestStruct
{
    int32_t id;
    std::string name;
    std::vector<float> values;
    // v2 added field
    std::string description;

    bool operator==(const VersionedTestStruct& other) const
    {
        return id == other.id && name == other.name && values == other.values && description == other.description;
    }
};

namespace kb
{
template <>
struct VersionedArchiver<VersionedTestStruct>
{
    static constexpr uint32_t k_current_version = 2;

    static bool write(const VersionedTestStruct& object, StreamSerializer& ser, uint32_t version)
    {
        bool success = ser.write(object.id) && ser.write(object.name) && ser.write(object.values);

        // Version 2 adds description field
        if (version >= 2)
        {
            success = success && ser.write(object.description);
        }

        return success;
    }

    static bool read(VersionedTestStruct& object, StreamDeserializer& des, uint32_t version)
    {
        bool success = des.read(object.id) && des.read(object.name) && des.read(object.values);

        // Version 2 adds description field
        if (version >= 2)
        {
            success = success && des.read(object.description);
        }
        else
        {
            // For older versions, set default value
            object.description = "";
        }

        return success;
    }
};

static_assert(VersionedSerializable<VersionedTestStruct>, "VersionedTestStruct cannot be automatically serialized.");
static_assert(VersionedDeserializable<VersionedTestStruct>,
              "VersionedTestStruct cannot be automatically deserialized.");

} // namespace kb

TEST_CASE("Trivial types serialization", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    SECTION("Arithmetic types")
    {
        // Integer types
        SECTION("int32_t")
        {
            int32_t original = -42;
            int32_t result = 0;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == original);
        }

        SECTION("uint32_t")
        {
            uint32_t original = 42;
            uint32_t result = 0;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == original);
        }

        SECTION("uint64_t")
        {
            uint64_t original = 0xDEADBEEFCAFEBABE;
            uint64_t result = 0;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == original);
        }

        // Floating point types
        SECTION("float")
        {
            float original = 3.14159f;
            float result = 0.0f;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == Catch::Approx(original));
        }

        SECTION("double")
        {
            double original = 2.71828182845904523536;
            double result = 0.0;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == Catch::Approx(original));
        }

        // Boolean
        SECTION("bool")
        {
            bool original = true;
            bool result = false;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == original);
        }
    }

    SECTION("Scoped enums")
    {
        enum class TestEnum : uint8_t
        {
            Value1 = 1,
            Value2 = 2,
            Value3 = 3
        };

        TestEnum original = TestEnum::Value2;
        TestEnum result = TestEnum::Value1;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("Trivial structs")
    {
        TrivialTestStruct original{42, 3.14159f, true};
        TrivialTestStruct result{};

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }
}

TEST_CASE("Blob serialization", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    SECTION("write_blob and read_blob")
    {
        uint8_t original_data[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        uint8_t result_data[16] = {};

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write_blob(original_data, sizeof(original_data)));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read_blob(result_data, sizeof(result_data)));
        }

        for (size_t i = 0; i < sizeof(original_data); ++i)
        {
            REQUIRE(result_data[i] == original_data[i]);
        }
    }
}

TEST_CASE("String serialization", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    SECTION("Empty string")
    {
        std::string original = "";
        std::string result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("ASCII string")
    {
        std::string original = "Hello, World!";
        std::string result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("UTF-8 string")
    {
        std::string original = "こんにちは世界! 😀 ñáéíóú";
        std::string result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }
}

TEST_CASE("Filesystem path serialization", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    SECTION("Simple path")
    {
        std::filesystem::path original = "folder/file.txt";
        std::filesystem::path result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("Absolute path")
    {
        std::filesystem::path original = "/usr/local/bin/app";
        std::filesystem::path result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("Windows-style path")
    {
        std::filesystem::path original = "C:\\Program Files\\App\\file.exe";
        std::filesystem::path result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        // Use generic_string() for comparison because Windows-style paths might be normalized differently
        REQUIRE(result.generic_string() == original.generic_string());
    }
}

TEST_CASE("STL container serialization", "[serialization]")
{
    // Test buffer
    char buffer[8192] = {}; // Larger buffer for containers

    SECTION("std::vector")
    {
        SECTION("vector of integers")
        {
            std::vector<int> original = {1, 2, 3, 4, 5};
            std::vector<int> result;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == original);
        }

        SECTION("vector of strings")
        {
            std::vector<std::string> original = {"hello", "world", "test", "vector"};
            std::vector<std::string> result;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == original);
        }
    }

    SECTION("std::array")
    {
        SECTION("array of integers")
        {
            std::array<int, 5> original = {1, 2, 3, 4, 5};
            std::array<int, 5> result = {};

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == original);
        }

        SECTION("array of custom structs")
        {
            std::array<CustomTestStruct, 2> original = {CustomTestStruct{1, "first", {1.0f, 2.0f}},
                                                        CustomTestStruct{2, "second", {3.0f, 4.0f}}};
            std::array<CustomTestStruct, 2> result = {};

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result == original);
        }
    }

    SECTION("std::pair")
    {
        std::pair<int, std::string> original{42, "answer"};
        std::pair<int, std::string> result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("std::tuple")
    {
        std::tuple<int, std::string, float> original{42, "answer", 3.14f};
        std::tuple<int, std::string, float> result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("std::unordered_map")
    {
        std::unordered_map<std::string, int> original = {{"one", 1}, {"two", 2}, {"three", 3}};
        std::unordered_map<std::string, int> result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        // Compare content (unordered_map doesn't implement operator==)
        REQUIRE(result.size() == original.size());
        for (const auto& [key, value] : original)
        {
            REQUIRE(result.contains(key));
            REQUIRE(result[key] == value);
        }
    }

    SECTION("std::unordered_set")
    {
        std::unordered_set<std::string> original = {"apple", "banana", "cherry"};
        std::unordered_set<std::string> result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        // Compare content (unordered_set doesn't implement operator==)
        REQUIRE(result.size() == original.size());
        for (const auto& item : original)
        {
            REQUIRE(result.contains(item));
        }
    }
}

TEST_CASE("Custom class serialization", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    SECTION("CustomTestStruct")
    {
        CustomTestStruct original{42, "test name", {1.0f, 2.0f, 3.0f, 4.0f}};
        CustomTestStruct result{};

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }
}

TEST_CASE("Versioned serialization", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    SECTION("Current version serialization")
    {
        VersionedTestStruct original{42, "test name", {1.0f, 2.0f, 3.0f}, "test description"};
        VersionedTestStruct result{};

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("Backward compatibility")
    {
        // Create a V1 format data (manually)
        VersionedTestStruct original{
            42,
            "test name",
            {1.0f, 2.0f, 3.0f},
            "ignored in v1" // This won't be written in v1 format
        };
        VersionedTestStruct result{};

        // Create a mock v1 serialized data
        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);

            // Write version tag and v1
            uint32_t version_tag = 0x53524556; // "VERS"
            uint32_t version = 1;
            REQUIRE(serializer.write(version_tag));
            REQUIRE(serializer.write(version));

            // Write v1 fields
            REQUIRE(serializer.write(original.id));
            REQUIRE(serializer.write(original.name));
            REQUIRE(serializer.write(original.values));
            // Note: description is not written for v1
        }

        // Read with current deserializer
        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        // Check that basic fields match
        REQUIRE(result.id == original.id);
        REQUIRE(result.name == original.name);
        REQUIRE(result.values == original.values);
        // But description should be default value for v1
        REQUIRE(result.description == "");
    }
}

TEST_CASE("Edge cases", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    SECTION("Empty containers")
    {
        SECTION("Empty vector")
        {
            std::vector<int> original;
            std::vector<int> result;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result.empty());
        }

        SECTION("Empty map")
        {
            std::unordered_map<std::string, int> original;
            std::unordered_map<std::string, int> result;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result.empty());
        }

        SECTION("Empty set")
        {
            std::unordered_set<std::string> original;
            std::unordered_set<std::string> result;

            {
                OutputMemoryStream out_stream(buffer, sizeof(buffer));
                kb::StreamSerializer serializer(out_stream);
                REQUIRE(serializer.write(original));
            }

            {
                InputMemoryStream in_stream(buffer, sizeof(buffer));
                kb::StreamDeserializer deserializer(in_stream);
                REQUIRE(deserializer.read(result));
            }

            REQUIRE(result.empty());
        }
    }

    SECTION("Seeking in streams")
    {
        int original1 = 42;
        float original2 = 3.14f;
        std::string original3 = "test string";

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);

            REQUIRE(serializer.write(original1));
            ssize_t float_pos = serializer.tell();
            REQUIRE(serializer.write(original2));
            REQUIRE(serializer.write(original3));

            // Go back and overwrite the float
            serializer.seek(float_pos);
            float new_value = 2.71828f;
            REQUIRE(serializer.write(new_value));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);

            int result1;
            float result2;
            std::string result3;

            REQUIRE(deserializer.read(result1));
            REQUIRE(result1 == original1);

            REQUIRE(deserializer.read(result2));
            REQUIRE(result2 == Catch::Approx(2.71828f)); // Check modified value

            REQUIRE(deserializer.read(result3));
            REQUIRE(result3 == original3);
        }
    }

    SECTION("Large data")
    {
        // Create a large vector
        std::vector<int> original(1000);
        for (size_t i = 0; i < original.size(); ++i)
        {
            original[i] = static_cast<int>(i);
        }

        std::vector<int> result;

        // Use a bigger buffer
        char large_buffer[16384] = {};

        {
            OutputMemoryStream out_stream(large_buffer, sizeof(large_buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(large_buffer, sizeof(large_buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("Nested containers")
    {
        std::vector<std::vector<int>> original = {{1, 2, 3}, {4, 5}, {6, 7, 8, 9}};

        std::vector<std::vector<int>> result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }
}

TEST_CASE("Complex composite structures", "[serialization]")
{
    // Test buffer
    char buffer[4096] = {};

    SECTION("Vector of custom objects")
    {
        std::vector<CustomTestStruct> original = {
            {1, "first", {1.1f, 1.2f}}, {2, "second", {2.1f, 2.2f, 2.3f}}, {3, "third", {3.1f}}};

        std::vector<CustomTestStruct> result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result == original);
    }

    SECTION("Map of custom objects")
    {
        std::unordered_map<std::string, CustomTestStruct> original = {
            {"one", {1, "first", {1.1f, 1.2f}}}, {"two", {2, "second", {2.1f, 2.2f}}}, {"three", {3, "third", {3.1f}}}};

        std::unordered_map<std::string, CustomTestStruct> result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result.size() == original.size());
        for (const auto& [key, value] : original)
        {
            REQUIRE(result.contains(key));
            REQUIRE(result.at(key) == value);
        }
    }

    SECTION("Complex nested structure")
    {
        // Map of string to pair of vector and custom struct
        std::unordered_map<std::string, std::pair<std::vector<int>, CustomTestStruct>> original = {
            {"key1", {{1, 2, 3}, {101, "name1", {1.1f, 1.2f}}}}, {"key2", {{4, 5}, {102, "name2", {2.1f}}}}};

        decltype(original) result;

        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);
            REQUIRE(serializer.write(original));
        }

        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        REQUIRE(result.size() == original.size());
        for (const auto& [key, value] : original)
        {
            REQUIRE(result.contains(key));
            REQUIRE(result.at(key).first == value.first);
            REQUIRE(result.at(key).second == value.second);
        }
    }
}

TEST_CASE("Error handling", "[serialization]")
{
    // Test buffer
    char buffer[64] = {}; // Intentionally small buffer

    SECTION("Write beyond buffer capacity")
    {
        std::vector<int> large_vector(100, 42); // Much larger than buffer

        OutputMemoryStream out_stream(buffer, sizeof(buffer));
        kb::StreamSerializer serializer(out_stream);

        // Should fail because buffer is too small
        REQUIRE_FALSE(serializer.write(large_vector));
        REQUIRE_FALSE(serializer.good());
    }

    SECTION("Read beyond buffer capacity")
    {
        // Try to read outside buffer
        InputMemoryStream in_stream(buffer, sizeof(buffer));
        in_stream.seekg(63);
        kb::StreamDeserializer deserializer(in_stream);

        uint32_t result;
        REQUIRE_FALSE(deserializer.read(result));
        REQUIRE_FALSE(deserializer.good());
    }
}

// Define a struct with mixed trivial and non-trivial members
struct MixedStruct
{
    int a;
    float b;
    std::string c;
    std::vector<int> d;

    bool operator==(const MixedStruct& other) const
    {
        return a == other.a && b == other.b && c == other.c && d == other.d;
    }
};

namespace kb
{
template <>
struct Archiver<MixedStruct>
{
    static bool write(const MixedStruct& object, StreamSerializer& ser)
    {
        return ser.write(object.a) && ser.write(object.b) && ser.write(object.c) && ser.write(object.d);
    }

    static bool read(MixedStruct& object, StreamDeserializer& des)
    {
        return des.read(object.a) && des.read(object.b) && des.read(object.c) && des.read(object.d);
    }
};
} // namespace kb

TEST_CASE("Mixed trivial and non-trivial types", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    MixedStruct original{42, 3.14f, "test string", {1, 2, 3, 4}};
    MixedStruct result{};

    {
        OutputMemoryStream out_stream(buffer, sizeof(buffer));
        kb::StreamSerializer serializer(out_stream);
        REQUIRE(serializer.write(original));
    }

    {
        InputMemoryStream in_stream(buffer, sizeof(buffer));
        kb::StreamDeserializer deserializer(in_stream);
        REQUIRE(deserializer.read(result));
    }

    REQUIRE(result == original);
}

// Example of a multi-version serialization to test backward/forward compatibility
struct MultiVersionStruct
{
    // v1 fields
    int32_t id;
    std::string name;

    // v2 fields
    float value;

    // v3 fields
    std::vector<std::string> tags;

    // For equality comparison
    bool operator==(const MultiVersionStruct& other) const
    {
        return id == other.id && name == other.name && value == other.value && tags == other.tags;
    }
};

namespace kb
{
template <>
struct VersionedArchiver<MultiVersionStruct>
{
    static constexpr uint32_t k_current_version = 3;

    static bool write(const MultiVersionStruct& object, StreamSerializer& ser, uint32_t version)
    {
        bool success = true;

        // v1 fields
        success = success && ser.write(object.id);
        success = success && ser.write(object.name);

        // v2+ fields
        if (version >= 2)
        {
            success = success && ser.write(object.value);
        }

        // v3+ fields
        if (version >= 3)
        {
            success = success && ser.write(object.tags);
        }

        return success;
    }

    static bool read(MultiVersionStruct& object, StreamDeserializer& des, uint32_t version)
    {
        bool success = true;

        // v1 fields
        success = success && des.read(object.id);
        success = success && des.read(object.name);

        // v2+ fields
        if (version >= 2)
        {
            success = success && des.read(object.value);
        }
        else
        {
            // Default for older versions
            object.value = 0.0f;
        }

        // v3+ fields
        if (version >= 3)
        {
            success = success && des.read(object.tags);
        }
        else
        {
            // Default for older versions
            object.tags.clear();
        }

        return success;
    }
};
} // namespace kb

TEST_CASE("Multi-version serialization", "[serialization]")
{
    // Test buffer
    char buffer[1024] = {};

    SECTION("v1 format")
    {
        MultiVersionStruct original{1, "test", 0.0f, {}}; // Only v1 fields initialized
        MultiVersionStruct result{};

        // Create v1 serialized data
        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);

            // Write version tag and v1
            uint32_t version_tag = 0x53524556; // "VERS"
            uint32_t version = 1;
            REQUIRE(serializer.write(version_tag));
            REQUIRE(serializer.write(version));

            // Write v1 fields only
            REQUIRE(serializer.write(original.id));
            REQUIRE(serializer.write(original.name));
        }

        // Read with current deserializer
        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        // Should match original with defaults for v2/v3 fields
        REQUIRE(result.id == original.id);
        REQUIRE(result.name == original.name);
        REQUIRE(result.value == 0.0f);
        REQUIRE(result.tags.empty());
    }

    SECTION("v2 format")
    {
        MultiVersionStruct original{1, "test", 3.14f, {}}; // v1+v2 fields initialized
        MultiVersionStruct result{};

        // Create v2 serialized data
        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);

            // Write version tag and v2
            uint32_t version_tag = 0x53524556; // "VERS"
            uint32_t version = 2;
            REQUIRE(serializer.write(version_tag));
            REQUIRE(serializer.write(version));

            // Write v1+v2 fields
            REQUIRE(serializer.write(original.id));
            REQUIRE(serializer.write(original.name));
            REQUIRE(serializer.write(original.value));
        }

        // Read with current deserializer
        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        // Should match original with defaults for v3 fields
        REQUIRE(result.id == original.id);
        REQUIRE(result.name == original.name);
        REQUIRE(result.value == original.value);
        REQUIRE(result.tags.empty());
    }

    SECTION("v3 format (current)")
    {
        MultiVersionStruct original{1, "test", 3.14f, {"tag1", "tag2"}}; // All fields initialized
        MultiVersionStruct result{};

        // Create v3 serialized data
        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);

            // Using the normal serializer should use current version
            REQUIRE(serializer.write(original));
        }

        // Read with current deserializer
        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        // Should match original completely
        REQUIRE(result == original);
    }

    SECTION("Future version compatibility")
    {
        MultiVersionStruct original{1, "test", 3.14f, {"tag1", "tag2"}}; // All fields initialized
        MultiVersionStruct result{};

        // Create "future" v4 serialized data
        {
            OutputMemoryStream out_stream(buffer, sizeof(buffer));
            kb::StreamSerializer serializer(out_stream);

            // Write version tag and v4
            uint32_t version_tag = 0x53524556; // "VERS"
            uint32_t version = 4;              // Future version
            REQUIRE(serializer.write(version_tag));
            REQUIRE(serializer.write(version));

            // Write all current fields
            REQUIRE(serializer.write(original.id));
            REQUIRE(serializer.write(original.name));
            REQUIRE(serializer.write(original.value));
            REQUIRE(serializer.write(original.tags));

            // Write some "future" field that current deserializer doesn't know about
            std::string future_field = "future data";
            REQUIRE(serializer.write(future_field));
        }

        // Read with current deserializer
        {
            InputMemoryStream in_stream(buffer, sizeof(buffer));
            kb::StreamDeserializer deserializer(in_stream);
            REQUIRE(deserializer.read(result));
        }

        // Should match original known fields
        REQUIRE(result.id == original.id);
        REQUIRE(result.name == original.name);
        REQUIRE(result.value == original.value);
        REQUIRE(result.tags == original.tags);

        // Note: future field is ignored since current deserializer doesn't know about it
    }
}