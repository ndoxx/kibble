#include "kibble/random/xor_shift.h"

#include "fmt/core.h"
#include <iomanip>
#include <sstream>
#include <vector>

using namespace kb;

// Helper function to print seed state
std::string seed_to_hex(const rng::XorShiftEngine::Seed& seed)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(16) << seed.state_[0] << " ";
    ss << std::setw(16) << seed.state_[1];
    return ss.str();
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    fmt::println("=== XorShift RNG Cross-Platform Consistency Test ===\n");

    // Test 1: Seed from single uint64_t
    fmt::println("TEST 1: Seeding from uint64_t");
    fmt::println("------------------------------");
    {
        uint64_t seed_value = 12345;
        rng::XorShiftEngine::Seed seed(seed_value);

        fmt::println("Input seed: {}", seed_value);
        fmt::println("Resulting state: {}", seed_to_hex(seed));
        fmt::println("  state_[0] = 0x{:016x}", seed.state_[0]);
        fmt::println("  state_[1] = 0x{:016x}", seed.state_[1]);

        rng::XorShiftEngine rng(seed);
        fmt::println("First 10 random numbers:");
        for (int i = 0; i < 10; ++i)
        {
            fmt::println("  {}: {}", i, rng.rand64());
        }
    }
    fmt::println("");

    // Test 2: Different seed values
    fmt::println("TEST 2: Multiple seed values");
    fmt::println("-----------------------------");
    {
        std::vector<uint64_t> seeds = {0, 1, 42, 12345, 67890, 0xDEADBEEF, 0xFFFFFFFFFFFFFFFFULL};

        for (auto seed_val : seeds)
        {
            rng::XorShiftEngine::Seed seed(seed_val);
            fmt::println("Seed {}: state = {}", seed_val, seed_to_hex(seed));

            rng::XorShiftEngine rng(seed);
            // The order of evaluation of function arguments is unspecified in C++
            // Don't call rng stuff directly as an argument
            uint64_t r1 = rng.rand64();
            uint64_t r2 = rng.rand64();
            uint64_t r3 = rng.rand64();
            fmt::println("  First 3 randoms: {} {} {}", r1, r2, r3);
        }
    }
    fmt::println("");

    // Test 3: Seed from string
    fmt::println("TEST 3: Seeding from string");
    fmt::println("---------------------------");
    {
        std::vector<const char*> seed_strings = {"12345:67890", "0:0", "18446744073709551615:18446744073709551615",
                                                 "1234567890123456789:9876543210987654321"};

        for (auto seed_str : seed_strings)
        {
            rng::XorShiftEngine::Seed seed(seed_str);
            fmt::println("Seed '{}': state = {}", seed_str, seed_to_hex(seed));
            fmt::println("  state_[0] = 0x{:016x}", seed.state_[0]);
            fmt::println("  state_[1] = 0x{:016x}", seed.state_[1]);

            rng::XorShiftEngine rng(seed);
            uint64_t r1 = rng.rand64();
            uint64_t r2 = rng.rand64();
            uint64_t r3 = rng.rand64();
            fmt::println("  First 3 randoms: {} {} {}", r1, r2, r3);
        }
    }
    fmt::println("");

    // Test 4: Direct state initialization
    fmt::println("TEST 4: Direct state initialization");
    fmt::println("------------------------------------");
    {
        std::vector<std::pair<uint64_t, uint64_t>> states = {{0x1234567890ABCDEFULL, 0xFEDCBA0987654321ULL},
                                                             {0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL},
                                                             {1, 1},
                                                             {0, 1}};

        for (const auto& [upper, lower] : states)
        {
            rng::XorShiftEngine::Seed seed(upper, lower);
            fmt::println("State [{:016x}, {:016x}]", upper, lower);
            fmt::println("  Seed state: {}", seed_to_hex(seed));

            rng::XorShiftEngine rng(seed);
            fmt::println("  First 5 randoms:");
            for (int i = 0; i < 5; ++i)
            {
                fmt::println("    {}: {}", i, rng.rand64());
            }
        }
    }
    fmt::println("");

    // Test 5: Seed copying and forking
    fmt::println("TEST 5: Seed copying and forking");
    fmt::println("---------------------------------");
    {
        rng::XorShiftEngine rng1(12345u);
        auto seed = rng1.get_seed();

        fmt::println("RNG1 seed: {}", seed_to_hex(seed));
        fmt::println("RNG1 generates:");
        for (int i = 0; i < 3; ++i)
        {
            fmt::println("  {}", rng1.rand64());
        }

        // Fork RNG
        rng::XorShiftEngine rng2(seed);
        fmt::println("RNG2 (forked) generates:");
        for (int i = 0; i < 3; ++i)
        {
            fmt::println("  {}", rng2.rand64());
        }

        fmt::println("RNG1 continues:");
        for (int i = 0; i < 3; ++i)
        {
            fmt::println("  {}", rng1.rand64());
        }
    }
    fmt::println("");

    // Test 6: Simulated UUID bytes as seed
    fmt::println("TEST 6: 16 bytes (UUID-like) as seed");
    fmt::println("--------------------------------------");
    {
        // Simulate UUID bytes being copied into seed
        uint8_t uuid_bytes[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                                  0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

        fmt::println("UUID bytes (hex): ");
        fmt::print("  ");
        for (int i = 0; i < 16; ++i)
        {
            fmt::print("{:02x} ", uuid_bytes[i]);
        }
        fmt::println("");

        rng::XorShiftEngine::Seed seed;
        memcpy(&seed.state_, uuid_bytes, 16);

        fmt::println("Seed state: {}", seed_to_hex(seed));
        fmt::println("  state_[0] = 0x{:016x}", seed.state_[0]);
        fmt::println("  state_[1] = 0x{:016x}", seed.state_[1]);

        rng::XorShiftEngine rng(seed);
        fmt::println("First 10 random numbers:");
        for (int i = 0; i < 10; ++i)
        {
            fmt::println("  {}: {}", i, rng.rand64());
        }
    }
    fmt::println("");

    // Test 7: Check splitmix64 intermediate values
    fmt::println("TEST 7: Splitmix64 internals");
    fmt::println("-----------------------------");
    {
        // Manually test what happens in Seed::Seed(uint64_t)
        uint64_t input = 12345;
        fmt::println("Input: {}", input);

        // Replicate the splitmix logic
        auto splitmix64_test = [](uint64_t& state) -> uint64_t {
            uint64_t result = state;
            state = result + 0x9E3779B97f4A7C15;
            result = (result ^ (result >> 30)) * 0xBF58476D1CE4E5B9;
            result = (result ^ (result >> 27)) * 0x94D049BB133111EB;
            return result ^ (result >> 31);
        };

        uint64_t smstate = input;
        uint64_t tmp1 = splitmix64_test(smstate);
        fmt::println("After first splitmix64:");
        fmt::println("  tmp = 0x{:016x}", tmp1);
        fmt::println("  smstate = 0x{:016x}", smstate);
        fmt::println("  uint32_t(tmp) = 0x{:08x}", uint32_t(tmp1));
        fmt::println("  uint32_t(tmp >> 32) = 0x{:08x}", uint32_t(tmp1 >> 32));

        uint64_t tmp2 = splitmix64_test(smstate);
        fmt::println("After second splitmix64:");
        fmt::println("  tmp = 0x{:016x}", tmp2);
        fmt::println("  smstate = 0x{:016x}", smstate);
        fmt::println("  uint32_t(tmp) = 0x{:08x}", uint32_t(tmp2));
        fmt::println("  uint32_t(tmp >> 32) = 0x{:08x}", uint32_t(tmp2 >> 32));

        // What SHOULD happen vs what the buggy code does
        fmt::println("\nWhat the buggy code produces:");
        fmt::println("  state_[0] = 0x{:016x} (overwrites first round)", uint64_t(uint32_t(tmp2)));
        fmt::println("  state_[1] = 0x{:016x} (overwrites first round)", uint64_t(uint32_t(tmp2 >> 32)));

        // Create actual seed and compare
        rng::XorShiftEngine::Seed actual_seed(input);
        fmt::println("\nActual seed from constructor:");
        fmt::println("  state_[0] = 0x{:016x}", actual_seed.state_[0]);
        fmt::println("  state_[1] = 0x{:016x}", actual_seed.state_[1]);
    }
    fmt::println("");

    fmt::println("=== Test Complete ===");
    fmt::println("Run this program on both Linux and Windows.");
    fmt::println("All output should be IDENTICAL between platforms.");

    return 0;
}