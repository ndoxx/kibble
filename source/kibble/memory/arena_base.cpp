#include "kibble/memory/arena_base.h"

namespace kb::memory
{

MemoryArenaBase::MemoryArenaBase(const std::string& name)
{
    // Truncate if needed, always leaving room for the null terminator
    size_t len = name.size() < (k_max_name_len - 1) ? name.size() : (k_max_name_len - 1);
    std::memcpy(name_, name.data(), len);
    name_[len] = '\0';
}

} // namespace kb::memory