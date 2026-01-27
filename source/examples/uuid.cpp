#include "kibble/random/uuid.h"
#include "kibble/platform/arch.h"
#include "kibble/random/xor_shift.h"

#include "fmt/core.h"
#include <iomanip>
#include <sstream>

using namespace kb;

// Helper function to print bytes as hex
std::string bytes_to_hex(const uint8_t* data, size_t len)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i)
    {
        ss << std::setw(2) << static_cast<int>(data[i]);
        if (i < len - 1)
        {
            ss << " ";
        }
    }
    return ss.str();
}

// Helper function to print seed state
std::string seed_to_hex(const rng::XorShiftEngine::Seed& seed)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(16) << seed.state_[0] << " ";
    ss << std::setw(16) << seed.state_[1];
    return ss.str();
}

// Create seed from UUID by copying bytes
inline rng::XorShiftEngine::Seed seed_from_uuid(const UUIDv4::UUID& uuid)
{
    rng::XorShiftEngine::Seed seed;
    memcpy(&seed.state_, uuid.data(), 16);
    return seed;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    fmt::println("=== UUID Cross-Platform Consistency Test ===\n");

    // Test 1: Fixed seed generation
    fmt::println("TEST 1: Fixed seed UUID generation");
    fmt::println("-----------------------------------");
    {
        UUIDv4::UUIDGenerator<rng::XorShiftEngine> gen(12345u);

        for (size_t ii = 0; ii < 5; ++ii)
        {
            auto uuid = gen();
            fmt::println("UUID {}: {}", ii, uuid.str());
            fmt::println("  Bytes: {}", bytes_to_hex(uuid.data(), 16));
        }
    }
    fmt::println("");

    // Test 2: UUID string round-trip
    fmt::println("TEST 2: UUID string serialization/deserialization");
    fmt::println("--------------------------------------------------");
    {
        UUIDv4::UUIDGenerator<rng::XorShiftEngine> gen(67890u);
        auto original = gen();
        std::string str_repr = original.str();
        auto parsed = UUIDv4::UUID::from_str_factory(str_repr);

        fmt::println("Original UUID: {}", str_repr);
        fmt::println("  Original bytes: {}", bytes_to_hex(original.data(), 16));
        fmt::println("Parsed UUID:   {}", parsed.str());
        fmt::println("  Parsed bytes:   {}", bytes_to_hex(parsed.data(), 16));
        fmt::println("  Match: {}", (original == parsed) ? "YES" : "NO");
    }
    fmt::println("");

    // Test 3: UUID bytes() round-trip
    fmt::println("TEST 3: UUID bytes() serialization/deserialization");
    fmt::println("---------------------------------------------------");
    {
        UUIDv4::UUIDGenerator<rng::XorShiftEngine> gen(11111u);
        auto original = gen();
        std::string byte_str = original.bytes();
        auto parsed = UUIDv4::UUID(byte_str);

        fmt::println("Original UUID: {}", original.str());
        fmt::println("  Original bytes: {}", bytes_to_hex(original.data(), 16));
        fmt::println("Parsed UUID:   {}", parsed.str());
        fmt::println("  Parsed bytes:   {}", bytes_to_hex(parsed.data(), 16));
        fmt::println("  Match: {}", (original == parsed) ? "YES" : "NO");
    }
    fmt::println("");

    // Test 4: Seeding RNG from UUID
    fmt::println("TEST 4: Seeding XorShift from UUID");
    fmt::println("-----------------------------------");
    {
        // Create a known UUID
        UUIDv4::UUIDGenerator<rng::XorShiftEngine> uuid_gen(99999u);
        auto seed_uuid = uuid_gen();

        fmt::println("Seed UUID: {}", seed_uuid.str());
        fmt::println("  UUID bytes: {}", bytes_to_hex(seed_uuid.data(), 16));

        // Convert to seed
        auto seed = seed_from_uuid(seed_uuid);
        fmt::println("  XorShift seed: {}", seed_to_hex(seed));

        // Generate some random numbers
        rng::XorShiftEngine rng(seed);
        fmt::println("  First 5 random numbers:");
        for (int i = 0; i < 5; ++i)
        {
            fmt::println("    {}: {}", i, rng.rand64());
        }
    }
    fmt::println("");

    // Test 5: UUID from specific string (simulating loaded data)
    fmt::println("TEST 5: Parse specific UUID strings");
    fmt::println("------------------------------------");
    {
        // These should be UUIDs you've serialized on Linux
        std::vector<std::string> test_uuids = {"a1b2c3d4-e5f6-4789-8abc-def012345678",
                                               "12345678-1234-4234-8234-567890abcdef",
                                               "00000000-0000-4000-8000-000000000001"};

        for (const auto& uuid_str : test_uuids)
        {
            auto uuid = UUIDv4::UUID::from_str_factory(uuid_str);
            fmt::println("Input:  {}", uuid_str);
            fmt::println("Parsed: {}", uuid.str());
            fmt::println("Bytes:  {}", bytes_to_hex(uuid.data(), 16));

            // Use as seed
            auto seed = seed_from_uuid(uuid);
            fmt::println("Seed:   {}", seed_to_hex(seed));

            // Generate first random number
            rng::XorShiftEngine rng(seed);
            fmt::println("First random: {}", rng.rand64());
            fmt::println("");
        }
    }

    // Test 6: Complete workflow - UUID -> seed -> new UUIDs
    fmt::println("TEST 6: Complete workflow (UUID as seed -> generate more UUIDs)");
    fmt::println("----------------------------------------------------------------");
    {
        // This simulates your import_prefab scenario
        auto master_uuid = UUIDv4::UUID::from_str_factory("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
        fmt::println("Master UUID: {}", master_uuid.str());
        fmt::println("  Bytes: {}", bytes_to_hex(master_uuid.data(), 16));

        // Seed generator from this UUID
        UUIDv4::UUIDGenerator<rng::XorShiftEngine> gen;
        auto seed = seed_from_uuid(master_uuid);
        gen.get_generator().seed(seed);

        fmt::println("  Seed: {}", seed_to_hex(seed));
        fmt::println("Generated child UUIDs:");
        for (int i = 0; i < 5; ++i)
        {
            auto child = gen();
            fmt::println("  Child {}: {}", i, child.str());
            fmt::println("    Bytes: {}", bytes_to_hex(child.data(), 16));
        }
    }
    fmt::println("");

    // Test 7: Check CPU features
    fmt::println("TEST 7: CPU Feature Detection");
    fmt::println("------------------------------");
    fmt::println("AVX2 Support: {}", CPUInfo::has_ISA_AVX2() ? "YES" : "NO");
    fmt::println("");

    fmt::println("=== Test Complete ===");
    fmt::println("Run this program on both Linux and Windows.");
    fmt::println("All output should be IDENTICAL between platforms.");

    return 0;
}