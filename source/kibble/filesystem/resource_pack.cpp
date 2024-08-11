#include "filesystem/resource_pack.h"
#include "assert/assert.h"
#include "filesystem/serialization/std_archiver.h"
#include "filesystem/serialization/stream_serializer.h"
#include "filesystem/stream/packfile_stream.h"
#include "logger/logger.h"

#include <fstream>

namespace kb::kfs
{

#define KPAK_MAGIC 0x4b41504b // ASCII(KPAK)
#define KPAK_VERSION_MAJOR 0
#define KPAK_VERSION_MINOR 2

struct Header
{
    uint32_t magic;         // Magic number to check file format validity
    uint16_t version_major; // Version major number
    uint16_t version_minor; // Version minor number
};

bool PackFileBuilder::check_ignore(const fs::path& dir_path)
{
    fs::path ignore_path = dir_path / "kpakignore";
    if (!fs::exists(ignore_path) || !fs::is_regular_file(ignore_path))
    {
        return false;
    }

    klog(log_channel_).uid("kpak").info("Detected kpakignore file.");
    std::ifstream ifs(ignore_path);
    if (!ifs)
    {
        klog(log_channel_).uid("kpakIgnore").error("Problem opening kpakignore file: {}", ignore_path.c_str());
        return false;
    }

    // Ignore the kpakignore file itself
    ignore_.insert("kpakignore"_h);

    // For each line in the file, store a hash
    std::string line;
    while (std::getline(ifs, line))
    {
        if (fs::exists(dir_path / line))
        {
            hash_t key = H_(line);
            if (ignore_.find(key) != ignore_.end())
            {
                klog(log_channel_)
                    .uid("kpakIgnore")
                    .warn("Duplicate kpakignore entry, or hash collision for:\n{}", line);
            }
            klog(log_channel_).uid("kpakIgnore").info("ignore: {}", line);
            ignore_.insert(key);
        }
    }

    return true;
}

bool PackFileBuilder::add_file(const fs::path& src, const fs::path& dst)
{
    if (!fs::exists(src) || !fs::is_regular_file(src))
    {
        return false;
    }

    hash_t key = H_(dst.c_str());

    if (pak_.index.contains(key))
    {
        klog(log_channel_).uid("kpak").warn("Skipping duplicate entry: {}", dst.c_str());
        return false;
    }

    // Read data
    std::ifstream ifs(src, std::ios::binary | std::ios::ate);
    if (!ifs)
    {
        return false;
    }

    klog(log_channel_).uid("kpak").info("Adding file: {}", dst.c_str());

    uint32_t size = uint32_t(ifs.tellg());
    ifs.seekg(0);
    data_.insert(data_.end(), std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

    // Add entry
    pak_.index.insert({key, PackFileIndex::Entry{.offset = current_offset_, .size = size}});
    current_offset_ += size;

    return true;
}

bool PackFileBuilder::add_directory(const fs::path& dir_path)
{
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path))
    {
        return false;
    }

    klog(log_channel_).uid("kpak").info("Adding directory: {}", dir_path.c_str());

    // Check for ignore list
    check_ignore(dir_path);

    for (auto& entry : fs::recursive_directory_iterator(dir_path))
    {
        fs::path rel_path = fs::relative(entry.path(), dir_path);
        if (!ignore_.contains(H_(rel_path.c_str())))
        {
            add_file(entry.path(), rel_path);
        }
    }

    return true;
}

bool PackFileBuilder::export_pack(std::ostream& stream)
{
    // Update offsets
    size_t initial_offset = export_size_bytes() - data_.size();

    for (auto& kvp : pak_.index)
    {
        kvp.second.offset += initial_offset;
    }

    StreamSerializer ser(stream);
    // clang-format off
    return ser.write(pak_) &&
           ser.write(data_);
    // clang-format on
}

size_t PackFileBuilder::export_size_bytes() const
{
    size_t map_size = sizeof(size_t) + pak_.index.size() * (sizeof(uint64_t) + sizeof(PackFileIndex::Entry));
    size_t blob_size = sizeof(size_t) + data_.size();
    return sizeof(Header) + map_size + blob_size;
}

PackFile::PackFile(std::shared_ptr<std::istream> stream) : base_stream_(stream)
{
    kb::StreamDeserializer des(*base_stream_);
    [[maybe_unused]] bool success = des.read(pak_);
    K_ASSERT(success, "Failed to read pack file from stream.");
}

std::shared_ptr<std::istream> PackFile::get_input_stream(kb::hash_t key) const
{
    if (auto findit = pak_.index.find(key); findit != pak_.index.end())
    {
        return std::make_shared<PackFileStream>(*base_stream_, findit->second.offset, findit->second.size);
    }
    return nullptr;
}

} // namespace kb::kfs

namespace kb
{

bool Archiver<kb::kfs::PackFileIndex>::write(const kb::kfs::PackFileIndex& object, StreamSerializer& ser)
{
    // clang-format off
    kb::kfs::Header header{
        .magic = KPAK_MAGIC, 
        .version_major = KPAK_VERSION_MAJOR, 
        .version_minor = KPAK_VERSION_MINOR
    };

    return ser.write(header) &&
           ser.write(object.index);
    // clang-format on
}

bool Archiver<kb::kfs::PackFileIndex>::read(kb::kfs::PackFileIndex& object, StreamDeserializer& des)
{
    kb::kfs::Header header;
    if (!des.read(header))
    {
        return false;
    }

    K_ASSERT(header.magic == KPAK_MAGIC, "Invalid KPAK file: magic number mismatch.\n  -> Expected: {}, got: {}",
             KPAK_MAGIC, header.magic);
    K_ASSERT(header.version_major == KPAK_VERSION_MAJOR,
             "Invalid KPAK file: version (major) mismatch.\n  -> Expected: {}, got: {}", KPAK_VERSION_MAJOR,
             header.version_major);
    K_ASSERT(header.version_minor == KPAK_VERSION_MINOR,
             "Invalid KPAK file: version (minor) mismatch.\n  -> Expected: {}, got: {}", KPAK_VERSION_MINOR,
             header.version_minor);

    return des.read(object.index);
}

} // namespace kb