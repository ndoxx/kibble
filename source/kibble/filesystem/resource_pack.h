#pragma once

#include <filesystem>
#include <vector>

#include "kibble/filesystem/serialization/archiver.h"
#include "kibble/hash/hash.h"
#include "kibble/util/unordered_dense.h"

namespace fs = std::filesystem;

namespace kb::log
{
class Channel;
}

namespace kb::kfs
{

/**
 * @brief Describe files inside a pack
 */
struct PackFileIndex
{
    struct Entry
    {

        uint32_t offset; // Byte offset in the pack
        uint32_t size;   // Size in bytes the file takes up
    };

    ankerl::unordered_dense::map<kb::hash_t, Entry> index;
};

/**
 * @brief Programatically build a pack file
 */
class PackFileBuilder
{
public:
    // clang-format off
    inline void set_logger(const kb::log::Channel* log_channel) { log_channel_ = log_channel; }
    // clang-format on

    /**
     * @brief Add file to the pack
     *
     * @param src Existing regular file path to source file
     * @param dst Virtual path inside the pack
     * @return success
     */
    bool add_file(const fs::path& src, const fs::path& dst);

    /**
     * @brief Recursively pack a directory's content.
     * If a "kpakignore" file is present at the root, all files listed in it (relative to the root)  will be skipped.
     * The "kpakignore" file itself will not be packed. The *kpack* utility makes this feature accessible through a
     * command line interface, allowing to pack directories from a script or the build system, for example.
     *
     * @param dir_path Path to the directory to pack
     * @return success
     */
    bool add_directory(const fs::path& dir_path);

    /**
     * @brief Export pack to output stream
     *
     * @param os Generic output stream (file stream, memory stream...)
     * @return success
     */
    bool export_pack(std::ostream& stream);

    /// @brief Total serialized size in bytes
    size_t export_size_bytes() const;

private:
    /// @brief Check and parse root kpakignore file
    bool check_ignore(const fs::path& dir_path);

private:
    PackFileIndex pak_;
    ankerl::unordered_dense::set<kb::hash_t> ignore_;
    std::vector<char> data_;
    uint32_t current_offset_{0};
    const kb::log::Channel* log_channel_{nullptr};
};

/**
 * @brief Allows to read files from a pack file.
 * A pack file is an archive-like binary file consisting of a header with an allocation table and multiple
 * concatenated files that are referenced in the allocation table.
 * A pack file can be obtained by recursively walking a given directory with the help of a `PackFileBuilder`.
 * Kibble comes with the *kpack* command line utility that facilitates this process. Once a pack file has been
 * generated, it is in read-only mode through this interface. If the content of the pack file should be updated,
 * then the whole directory must be repacked.
 * This class uses a custom input stream to access the individual files, as if they were separate files on
 * the disk.
 *
 */
class PackFile
{
public:
    /**
     * @brief Construct a new Pack File from a stream.
     * The constructor uses assertions to make sure the file exists and the header is valid, so it is important to
     * build and test your application in debug mode first, where this assertion will be evaluated.
     * Packs created with a different version of this class are incompatible and cannot be read when the assertions are
     * enabled. If the assertions are disabled, anything is possible, so beware.\n
     * When a pack file is read, the index structure is filled with entries referencing each file.
     * Each entry is associated to a key in the index, the key being the hash of the relative path.
     *
     * @param filepath Path to the pack file
     */
    PackFile(std::shared_ptr<std::istream> stream);

    /**
     * @brief Get an input stream pointer to a file.
     *
     * @param path Relative path of the file inside the pack
     * @return Input stream pointer, nullptr if not found
     */
    std::shared_ptr<std::istream> get_input_stream(kb::hash_t key) const;

    // clang-format off
    inline auto begin() const { return pak_.index.begin(); }
    inline auto end() const   { return pak_.index.end(); }
    inline std::shared_ptr<std::istream> get_input_stream(std::string_view sv) const { return get_input_stream(H_(sv)); };
    inline const auto& get_entry(kb::hash_t key) const { return pak_.index.at(key); }
    inline const auto& get_entry(std::string_view sv) const { return pak_.index.at(H_(sv)); }
    // clang-format on

private:
    PackFileIndex pak_;
    std::shared_ptr<std::istream> base_stream_;
};

} // namespace kb::kfs

namespace kb
{
template <>
struct Archiver<kb::kfs::PackFileIndex>
{
    static bool write(const kb::kfs::PackFileIndex& object, StreamSerializer& ser);
    static bool read(kb::kfs::PackFileIndex& object, StreamDeserializer& des);
};
} // namespace kb