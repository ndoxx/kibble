#pragma once

#include <cstddef>
#include <cstring>
#include <string>

namespace kb::memory
{

/**
 * @brief Thin base for memory arenas
 *
 */
class MemoryArenaBase
{
public:
    /// @brief Max length of the debug name, including null terminator
    static constexpr size_t k_max_name_len = 64;

    MemoryArenaBase(const std::string& name);

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
    char name_[k_max_name_len];
};

} // namespace kb::memory