#include "filesystem/resource_pack.h"
#include "assert/assert.h"
#include "logger2/logger.h"
#include "memory/memory_utils.h"
#include "string/string.h"

#include <array>
#include <cmath>
#include <fmt/std.h>
#include <fstream>
#include <set>
#include <vector>

namespace kb::kfs
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
    if (avail)
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

std::set<hash_t> parse_kpakignore(const fs::path& filepath, const kb::log::Channel* log_channel)
{
    K_ASSERT(fs::is_regular_file(filepath), "kpakignore is not a file.", log_channel).watch(filepath);
    fs::path base_path = filepath.parent_path();

    std::ifstream ifs(filepath);
    K_ASSERT(ifs.is_open(), "Problem opening kpakignore file.", log_channel).watch(filepath);

    // For each line in the file, store a hash
    std::set<hash_t> result;
    std::string line;
    while (std::getline(ifs, line))
    {
        if (fs::exists(base_path / line))
        {
            hash_t key = H_(line);
            if (result.find(key) != result.end())
            {
                klog(log_channel)
                    .uid("kpakIgnore")
                    .warn("Duplicate kpakignore entry, or hash collision for:\n{}", line);
            }
            klog(log_channel).uid("kpakIgnore").info("ignore: {}", line);
            result.insert(key);
        }
    }
    // Ignore the kpakignore file itself
    result.insert(H_(filepath.stem()));
    return result;
}

bool PackFile::pack_directory(const fs::path& dir_path, const fs::path& archive_path,
                              const kb::log::Channel* log_channel)
{
    if (!fs::exists(dir_path))
    {
        klog(log_channel).uid("kpak").error("Directory does not exist:\n{}", dir_path);
        return false;
    }

    // * First, try to find a kpakignore file and list all files that should be ignored
    std::set<hash_t> ignored;
    if (fs::exists(dir_path / "kpakignore"))
    {
        klog(log_channel).uid("kpak").info("Detected kpakignore file.");
        ignored = parse_kpakignore(dir_path / "kpakignore", log_channel);
    }

    PAKHeader h;
    h.magic = KPAK_MAGIC;
    h.version_major = KPAK_VERSION_MAJOR;
    h.version_minor = KPAK_VERSION_MINOR;

    std::vector<PackLocalEntry> entries;
    size_t current_offset = sizeof(PAKHeader);
    std::uintmax_t max_file_size = 0;

    // * Explore directory recursively and build the pack list
    for (auto& entry : fs::recursive_directory_iterator(dir_path))
    {
        if (entry.is_regular_file())
        {
            fs::path rel_path = fs::relative(entry.path(), dir_path);

            // Do we ignore this file?
            hash_t key = H_(rel_path);
            if (ignored.find(key) != ignored.end())
                continue;

            current_offset += PackLocalEntry::k_serialized_size + rel_path.string().size();
            entries.push_back({0, uint32_t(entry.file_size()), rel_path.string()});
            if (entry.file_size() > max_file_size)
                max_file_size = entry.file_size();
        }
    }

    h.entry_count = uint32_t(entries.size());

    klog(log_channel).uid("kpak").info("Packing directory: {}", dir_path);
    klog(log_channel).uid("kpak").info("Target archive:    {}", archive_path);

    // Write header
    std::ofstream ofs(archive_path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&h), sizeof(PAKHeader));

    // Write index
    for (auto& entry : entries)
    {
        entry.offset = uint32_t(current_offset);
        entry.write(ofs);
        current_offset += size_t(entry.size);
    }

    // Write all files to pack
    size_t progress = 0;
    std::vector<char> databuf;
    databuf.reserve(size_t(max_file_size));
    for (const auto& entry : entries)
    {
        size_t progess_percent = size_t(std::round(100.f * float(++progress) / float(entries.size())));

        klog(log_channel)
            .uid("kpak")
            .verbose("{:3}% pack: {} ({})", progess_percent, entry.path, kb::memory::utils::human_size(entry.size));

        std::ifstream ifs(dir_path / entry.path, std::ios::binary);
        databuf.insert(databuf.begin(), std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
        ofs.write(databuf.data(), long(databuf.size()));
        databuf.clear();
    }

    return true;
}

PackFile::PackFile(const fs::path& filepath, const kb::log::Channel* log_channel)
    : filepath_(filepath), log_channel_(log_channel)
{
    K_ASSERT(fs::exists(filepath_), "Pack file does not exist.", log_channel_).watch(filepath_);
    std::ifstream ifs(filepath_, std::ios::binary);

    // Read header & sanity check
    PAKHeader h;
    ifs.read(reinterpret_cast<char*>(&h), sizeof(PAKHeader));
    K_ASSERT(h.magic == KPAK_MAGIC, "Invalid KPAK file: magic number mismatch.", log_channel_).watch(h.magic);
    K_ASSERT(h.version_major == KPAK_VERSION_MAJOR, "Invalid KPAK file: version (major) mismatch.", log_channel_)
        .watch(h.version_major);
    K_ASSERT(h.version_minor == KPAK_VERSION_MINOR, "Invalid KPAK file: version (minor) mismatch.", log_channel_)
        .watch(h.version_minor);

    // Read index
    for (size_t ii = 0; ii < h.entry_count; ++ii)
    {
        PackLocalEntry entry;
        entry.read(ifs);
        index_.insert({H_(entry.path), std::move(entry)});
    }
}

std::shared_ptr<std::istream> PackFile::get_input_stream(const PackLocalEntry& entry) const
{
    return std::make_shared<PackInputStream>(filepath_, entry);
}

} // namespace kb::kfs