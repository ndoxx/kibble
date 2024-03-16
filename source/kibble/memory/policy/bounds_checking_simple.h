#pragma once

#include <cstdint>
#include <algorithm>

#include "../../assert/assert.h"
#include "policy.h"

namespace kb::memory::policy
{

/**
 * @brief Bounds checking sanitization policy used by MemoryArena.
 * This implementation will write 4-bytes sentinels at the supplied locations, and will be able to check if the
 * sentinels are still intact after some time, which allows to track memory overwrite bugs.
 *
 */
class SimpleBoundsChecking
{
public:
    static constexpr size_t k_sentinel_size = 8;
    static constexpr size_t k_sentinel_front = 0xf0f0f0f0f0f0f0f0;
    static constexpr size_t k_sentinel_back = 0x0f0f0f0f0f0f0f0f;

    /**
     * @brief Write a 4-bytes front sentinel starting at ptr
     *
     * @param ptr start address to write to
     */
    inline void put_sentinel_front(uint8_t* ptr) const
    {
        std::fill(ptr, ptr + k_sentinel_size, 0xf0);
    }

    /**
     * @brief Write a 4-bytes back sentinel starting at ptr
     *
     * @param ptr start address to write to
     */
    inline void put_sentinel_back(uint8_t* ptr) const
    {
        std::fill(ptr, ptr + k_sentinel_size, 0x0f);
    }

    /**
     * @brief Assert that the front sentinel is intact
     *
     * @param ptr sentinel location
     */
    inline void check_sentinel_front(uint8_t* ptr) const
    {
        K_ASSERT(*reinterpret_cast<size_t*>(ptr) == k_sentinel_front, "Memory overwrite detected (front)", nullptr)
            .watch(static_cast<void*>(ptr))
            .watch(*reinterpret_cast<size_t*>(ptr));
    }

    /**
     * @brief Assert that the back sentinel is intact
     *
     * @param ptr sentinel location
     */
    inline void check_sentinel_back(uint8_t* ptr) const
    {
        K_ASSERT(*reinterpret_cast<size_t*>(ptr) == 0x0f0f0f0f0f0f0f0f, "Memory overwrite detected (back)", nullptr)
            .watch(static_cast<void*>(ptr))
            .watch(*reinterpret_cast<size_t*>(ptr));
    }
};

template <>
struct BoundsCheckerSentinelSize<SimpleBoundsChecking>
{
    static constexpr std::size_t FRONT = SimpleBoundsChecking::k_sentinel_size;
    static constexpr std::size_t BACK = SimpleBoundsChecking::k_sentinel_size;
};

}