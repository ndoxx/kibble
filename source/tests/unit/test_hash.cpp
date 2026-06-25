#include "kibble/hash/hash.h"

#include <catch2/catch_all.hpp>
#include <cmath>

TEST_CASE("Reversible hashes are reversible", "[hash]")
{
    auto h32 = kb::hakz::rev_hash_32(123456);
    auto h64 = kb::hakz::rev_hash_64(123456789101112);
    REQUIRE(kb::hakz::rev_unhash_32(h32) == 123456);
    REQUIRE(kb::hakz::rev_unhash_64(h64) == 123456789101112);
}

TEST_CASE("Epsilon hash works", "[hash]")
{
    auto h1 = kb::hakz::epsilon_hash(1.23456f);
    auto h2 = kb::hakz::epsilon_hash(std::nextafterf(1.23456f, 2.f));
    REQUIRE(h1 == h2);
}