#pragma once

#include <filesystem>
#include <map>

#include "../assert/assert.h"
#include "../hash/hashstr.h"

namespace fs = std::filesystem;
namespace kb
{
namespace kfs
{

struct PackLocalEntry
{
    uint32_t offset;
    uint32_t size;
    std::string path;

    static constexpr size_t k_serialized_size = 3 * sizeof(uint32_t);

    void write(std::ostream& stream);
    void read(std::istream& stream);
};

class PackFile
{
public:
    PackFile(const fs::path& filepath);

    inline std::shared_ptr<std::istream> get_input_stream(const std::string& path) const;
    std::shared_ptr<std::istream> get_input_stream(const PackLocalEntry& entry) const;

    inline auto find(const std::string& path) const { return index_.find(H_(path)); }
    inline auto begin() const { return index_.begin(); }
    inline auto end() const { return index_.end(); }
    inline const PackLocalEntry& get_entry(const std::string& path) const;
    inline const PackLocalEntry& get_entry(hash_t key) const;

    // Recursively pack a directory's content.
    // If a "kpakignore" file is present at the root, all files listed in it (relative to the root)
    // will be skipped. The "kpakignore" file itself will not be packed.
    static bool pack_directory(const fs::path& dir_path, const fs::path& archive_path);

private:
    fs::path filepath_;
    std::map<hash_t, PackLocalEntry> index_;
};

inline std::shared_ptr<std::istream> PackFile::get_input_stream(const std::string& path) const
{
    return get_input_stream(get_entry(path));
}

inline const PackLocalEntry& PackFile::get_entry(const std::string& path) const { return get_entry(H_(path)); }

inline const PackLocalEntry& PackFile::get_entry(hash_t key) const
{
    auto findit = index_.find(key);
    K_ASSERT(findit != index_.end(), "Unknown entry.");
    return findit->second;
}

} // namespace kfs
} // namespace kb