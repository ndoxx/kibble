#pragma once

#include <filesystem>
#include <map>

#include "../hash/hashstr.h"

namespace fs = std::filesystem;
namespace kb
{
namespace kfs
{

bool pack_directory(const fs::path& dir_path, const fs::path& archive_path);

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

    const PackLocalEntry& get_entry(const std::string& path) const;
    std::shared_ptr<std::istream> get_input_stream(const PackLocalEntry& entry) const;

    inline std::shared_ptr<std::istream> get_input_stream(const std::string& path) const
    {
        return get_input_stream(get_entry(path));
    }
    inline auto find(const std::string& path) const { return index_.find(H_(path)); }
    inline auto begin() const { return index_.begin(); }
    inline auto end() const { return index_.end(); }

private:
    fs::path filepath_;
    std::map<hash_t, PackLocalEntry> index_;
};

} // namespace kfs
} // namespace kb