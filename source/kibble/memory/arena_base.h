#pragma once

#include <cstddef>

namespace kb::memory
{

/**
 * @brief Thin base for memory arenas
 *
 */
class MemoryArenaBase
{
public:
    MemoryArenaBase(const char* name);
    virtual ~MemoryArenaBase() = default;

    /// @brief Get total size in bytes
    virtual size_t total_size() const = 0;
    /// @brief Get used size in bytes
    virtual size_t used_size() const = 0;
    /// @brief Get remaining size in bytes
    inline size_t free_size() const
    {
        return total_size() - used_size();
    }

    /// @brief Arena debug name
    const char* name_{nullptr};
};

} // namespace kb::memory