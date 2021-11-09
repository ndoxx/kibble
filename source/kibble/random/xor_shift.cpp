#include "assert/assert.h"
#include "random/xor_shift.h"
#include "string/string.h"

#include <chrono>

namespace kb
{
namespace rng
{

// For default initialization of XorShiftEngine
uint64_t splitmix64(uint64_t &state)
{
    uint64_t result = state;

    state = result + 0x9E3779B97f4A7C15;
    result = (result ^ (result >> 30)) * 0xBF58476D1CE4E5B9;
    result = (result ^ (result >> 27)) * 0x94D049BB133111EB;
    return result ^ (result >> 31);
}

XorShiftEngine::Seed::Seed(const char *str)
{
    auto tokens = kb::su::tokenize(str, ':');
    K_ASSERT(tokens.size() == 2, "[XorShiftEngine] Bad seed string.");
    state_[0] = std::strtoull(tokens[0].c_str(), nullptr, 10);
    state_[1] = std::strtoull(tokens[1].c_str(), nullptr, 10);
}

/*
    Quote from Wikipedia:
    It is the recommendation of the authors of the xoshiro paper to initialize
    the state of the generators using a generator which is radically different
    from the initialized generators, as well as one which will never give the
    "all-zero" state; for shift-register generators, this state is impossible to
    escape from. The authors specifically recommend using the SplitMix64 generator.
*/
XorShiftEngine::Seed::Seed(uint64_t seed)
{
    // Two rounds of splitmix to avoid the zero-seed situation
    uint64_t smstate = seed;
    uint64_t tmp = splitmix64(smstate);
    state_[0] = uint32_t(tmp);
    state_[1] = uint32_t((tmp >> 32));
    tmp = splitmix64(smstate);
    state_[0] = uint32_t(tmp);
    state_[1] = uint32_t((tmp >> 32));
}

std::ostream &operator<<(std::ostream &stream, XorShiftEngine::Seed rhs)
{
    stream << "[" << rhs.state_[0] << "," << rhs.state_[1] << "]";
    return stream;
}

XorShiftEngine::XorShiftEngine()
{
    seed();
}

uint64_t XorShiftEngine::rand64()
{
    uint64_t t = seed_.state_[0];
    const uint64_t s = seed_.state_[1];
    seed_.state_[0] = s;
    t ^= t << 23;
    seed_.state_[1] = t ^ s ^ (t >> 17) ^ (s >> 26);
    return seed_.state_[1] + s;
}

void XorShiftEngine::seed()
{
    using namespace std::chrono;
    uint64_t epoch_ms = uint64_t(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
    seed(epoch_ms);
}

} // namespace rng
} // namespace kb