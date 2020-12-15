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

/* Custom input stream and stream buffer implementations.
 * I don't really like the way I did it, but it was the simplest solution I could come up with.
 * PackInputStreambuf is a wrapper around a std::filebuf, and overrides the underflow()
 * method in such a way that EOF is returned when the end of the file segment is reached.
 * PackInputStream is a simple istream with a member PackInputStreambuf initialized during
 * construction.
 */
class PackInputStreambuf : public std::streambuf
{
public:
    PackInputStreambuf(const fs::path& filepath, const PackLocalEntry& entry);
    virtual ~PackInputStreambuf() = default;

protected:
    virtual int_type underflow() override;

private:
    std::filebuf buf_;
    PackLocalEntry entry_;
    std::streamsize remaining_;
    std::array<char, 1024> data_in_;
};

PackInputStreambuf::PackInputStreambuf(const fs::path& filepath, const PackLocalEntry& entry)
    : entry_(entry), remaining_(entry.size)
{
    buf_.open(filepath, std::ios::in | std::ios::binary);
    buf_.pubseekpos(entry_.offset, std::ios::in);
}

std::streambuf::int_type PackInputStreambuf::underflow()
{
    std::streamsize avail = std::min(long(data_in_.size()), remaining_);
    if(avail)
    {
        std::streamsize count = buf_.sgetn(&data_in_[0], avail);
        setg(&data_in_[0], &data_in_[0], &data_in_[size_t(count)]);
        remaining_ -= count;
        return traits_type::to_int_type(*gptr());
    }
    return traits_type::eof();
}

class PackInputStream : public std::istream
{
public:
    PackInputStream(const fs::path& filepath, const PackLocalEntry& entry);
    virtual ~PackInputStream() = default;

private:
    PackInputStreambuf buf_;
};

PackInputStream::PackInputStream(const fs::path& filepath, const PackLocalEntry& entry)
    : std::istream(nullptr), buf_(filepath, entry)
{
    init(&buf_);
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

const PackLocalEntry& PackFile::get_entry(const std::string& path) const
{
    hash_t hname = H_(path);
    auto findit = index_.find(hname);
    K_ASSERT(findit != index_.end(), "Unknown entry.");
    return findit->second;
}

std::shared_ptr<std::istream> PackFile::get_input_stream(const PackLocalEntry& entry) const
{
    return std::make_shared<PackInputStream>(filepath_, entry);
}


} // namespace kfs
} // namespace kb