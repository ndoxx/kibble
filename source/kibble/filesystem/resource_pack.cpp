#include "filesystem/resource_pack.h"
#include "assert/assert.h"
#include "logger/logger.h"
#include "string/string.h"

#include <array>
#include <fstream>
#include <vector>

namespace kb
{
namespace kfs
{

class PackInputStreambuf : public std::streambuf
{
public:
    PackInputStreambuf(const fs::path& filepath, const PackLocalEntry& entry);

protected:
    virtual int_type underflow() override;

private:
    std::ifstream ifs_;
    std::array<char, 1024> data_;
    std::streampos begin_ = 0;
    std::streamsize remaining_ = 0;
};

PackInputStreambuf::PackInputStreambuf(const fs::path& filepath, const PackLocalEntry& entry)
    : ifs_(filepath, std::ios::binary), begin_(entry.offset), remaining_(entry.size)
{
    ifs_.seekg(entry.offset);
}

std::streambuf::int_type PackInputStreambuf::underflow()
{
    if(remaining_ <= 0)
        return traits_type::eof();

    std::streamsize max_avail = std::min(long(data_.size()), remaining_);
    std::streamsize count = ifs_.readsome(&data_[0], max_avail);
    setg(&data_[0], &data_[0], &data_[size_t(count)]);
    remaining_ -= count;
    return traits_type::to_int_type(*gptr());
}

struct PAKHeader
{
    uint32_t magic;         // Magic number to check file format validity
    uint16_t version_major; // Version major number
    uint16_t version_minor; // Version minor number
    uint32_t entry_count;   // Number of file entries in this pack
};

void PackLocalEntry::write(std::ostream& stream)
{
    uint32_t path_len = uint32_t(path.size());
    stream.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
    stream.write(reinterpret_cast<const char*>(&path_len), sizeof(path_len));
    stream.write(reinterpret_cast<const char*>(path.data()), long(path.size()));
}

void PackLocalEntry::read(std::istream& stream)
{
    uint32_t path_len = 0;
    stream.read(reinterpret_cast<char*>(&offset), sizeof(offset));
    stream.read(reinterpret_cast<char*>(&size), sizeof(size));
    stream.read(reinterpret_cast<char*>(&path_len), sizeof(path_len));
    path.resize(path_len);
    stream.read(path.data(), long(path_len));
}

#define KPAK_MAGIC 0x4b41504b // ASCII(KPAK)
#define KPAK_VERSION_MAJOR 0
#define KPAK_VERSION_MINOR 1

bool pack_directory(const fs::path& dir_path, const fs::path& archive_path)
{
    if(!fs::exists(dir_path))
    {
        KLOGE("ios") << "[kpak] Directory does not exist:" << std::endl;
        KLOGI << WCC('p') << dir_path << std::endl;
        return false;
    }

    PAKHeader h;
    h.magic = KPAK_MAGIC;
    h.version_major = KPAK_VERSION_MAJOR;
    h.version_minor = KPAK_VERSION_MINOR;

    std::vector<PackLocalEntry> entries;
    size_t current_offset = sizeof(PAKHeader);
    std::uintmax_t max_file_size = 0;

    // Explore
    for(auto& entry : fs::recursive_directory_iterator(dir_path))
    {
        if(entry.is_regular_file())
        {
            fs::path rel_path = fs::relative(entry.path(), dir_path);
            current_offset += PackLocalEntry::k_serialized_size + rel_path.string().size();
            entries.push_back({0, uint32_t(entry.file_size()), rel_path.string()});
            if(entry.file_size() > max_file_size)
                max_file_size = entry.file_size();
        }
    }

    h.entry_count = uint32_t(entries.size());

    KLOG("ios", 0) << "[kpak] Packing directory:" << std::endl;
    KLOGI << WCC('p') << dir_path << std::endl;
    KLOG("ios", 0) << "[kpak] Target archive:" << std::endl;
    KLOGI << WCC('p') << archive_path << std::endl;

    // Write header
    std::ofstream ofs(archive_path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&h), sizeof(PAKHeader));

    // Write index
    for(auto& entry : entries)
    {
        entry.offset = uint32_t(current_offset);
        entry.write(ofs);
        current_offset += size_t(entry.size);
    }

    // Write all files to pack
    std::vector<char> databuf;
    databuf.reserve(size_t(max_file_size));
    for(const auto& entry : entries)
    {
        KLOG("ios", 0) << "[kpak] " << entry.path << " (" << su::size_to_string(entry.size) << ')' << std::endl;
        std::ifstream ifs(dir_path / entry.path, std::ios::binary);
        databuf.insert(databuf.begin(), std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
        ofs.write(databuf.data(), long(databuf.size()));
        databuf.clear();
    }

    return true;
}

PackFile::PackFile(const fs::path& filepath) : filepath_(filepath)
{
    K_ASSERT(fs::exists(filepath_), "Pack file does not exist.");
    std::ifstream ifs(filepath_, std::ios::binary);

    // Read header & sanity check
    PAKHeader h;
    ifs.read(reinterpret_cast<char*>(&h), sizeof(PAKHeader));
    K_ASSERT(h.magic == KPAK_MAGIC, "Invalid KPAK file: magic number mismatch.");
    K_ASSERT(h.version_major == KPAK_VERSION_MAJOR, "Invalid KPAK file: version (major) mismatch.");
    K_ASSERT(h.version_minor == KPAK_VERSION_MINOR, "Invalid KPAK file: version (minor) mismatch.");

    // Read index
    for(size_t ii = 0; ii < h.entry_count; ++ii)
    {
        PackLocalEntry entry;
        entry.read(ifs);
        index_.insert({H_(entry.path), std::move(entry)});
    }
}

const PackLocalEntry& PackFile::get_entry(const std::string& path)
{
    hash_t hname = H_(path);
    auto findit = index_.find(hname);
    K_ASSERT(findit != index_.end(), "Unknown entry.");
    return findit->second;
}

std::shared_ptr<std::streambuf> PackFile::get_input_streambuf(const std::string& path)
{
    return std::make_shared<PackInputStreambuf>(filepath_, get_entry(path));
}

} // namespace kfs
} // namespace kb