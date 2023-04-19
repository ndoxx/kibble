#pragma once

#include <filesystem>
#include <unordered_map>

#include "../assert/assert.h"
#include "../hash/hash.h"

namespace fs = std::filesystem;

namespace kb::log
{
class Channel;
}

namespace kb::kfs
{

/**
 * @brief Describe a file inside a pack
 */
struct PackLocalEntry
{
    /// Byte offset in the pack
    uint32_t offset;
    /// Size in bytes the file takes up
    uint32_t size;
    /// Relative path of this file
    std::string path;

    static constexpr size_t k_serialized_size = 3 * sizeof(uint32_t);

    /**
     * @brief Write this pack local entry to the allocation table at current stream position
     *
     * @param stream The output stream
     */
    void write(std::ostream& stream);

    /**
     * @brief Read a pack local entry from the allocation table at current stream position
     *
     * @param stream The input stream
     */
    void read(std::istream& stream);
};

/**
 * @brief Allows to read files from a pack file.
 * A pack file is an archive-like binary file consisting of a header with an allocation table and multiple
 * concatenated files that are referenced in the allocation table.
 * A pack file can be obtained by recursively walking a given directory. Kibble comes with the *kpack* command line
 * utility that facilitates this process. Once a pack file has been generated, it is in read-only mode through this
 * interface. If the content of the pack file should be updated, then the whole directory must be repacked.
 * This class uses an opaque input stream with a custom streambuffer implementation. The input stream inherits from the
 * std::istream class and is expected to be used polymorphically. That's why it is returned as a shared_pointer.
 *
 */
class PackFile
{
public:
    /**
     * @brief Construct a new Pack File from a file.
     * The constructor uses K_ASSERTs to make sure the file exists and the header is valid, so it is important to
     * build and test your application in debug mode first, where this assertion will be evaluated.
     * Packs created with a different version of this class are incompatible and cannot be read when the assertions are
     * enabled. If the assertions are disabled, anything is possible, so beware.\n
     * When a pack file is read, the index structure is filled with PackLocalEntry objects referencing each file entry.
     * Each entry is associated to a key in the index, the key being the hash of the relative path. These entries can be
     * retrieved by path or by hash later on.
     *
     * @param filepath Path to the pack file
     */
    PackFile(const fs::path& filepath, const kb::log::Channel* log_channel = nullptr);

    /**
     * @brief Get an input stream pointer to a file.
     *
     * @param path Relative path of the file inside the pack
     * @return std::shared_ptr<std::istream> Input stream pointer
     */
    inline std::shared_ptr<std::istream> get_input_stream(const std::string& path) const;

    /**
     * @brief Get an input stream pointer to a file.
     *
     * @param entry Pack local entry referencing the file
     * @return std::shared_ptr<std::istream> Input stream pointer
     */
    std::shared_ptr<std::istream> get_input_stream(const PackLocalEntry& entry) const;

    /**
     * @brief Get an iterator to the index entry corresponding to a file.
     *
     * @param path Relative path of the file inside the pack
     * @return A map iterator. The first element is a hash, the second is a PackLocalEntry referencing the file. If no
     * file was found, the map's std::unordered_map<hash_t, PackLocalEntry>::end() iterator will be returned.
     */
    inline auto find(const std::string& path) const
    {
        return index_.find(H_(path));
    }

    /**
     * @brief Return an iterator to the beginning of the index.
     * This makes a PackFile iterable.
     *
     * @return auto
     * @see end()
     */
    inline auto begin() const
    {
        return index_.begin();
    }

    /**
     * @brief Return an iterator past the last element of the index.
     * This makes a PackFile iterable.
     *
     * @return auto
     * @see begin()
     */
    inline auto end() const
    {
        return index_.end();
    }

    /**
     * @brief Get a PackLocalEntry for a given path
     *
     * @param path Relative path to the target file
     * @return const PackLocalEntry&
     */
    inline const PackLocalEntry& get_entry(const std::string& path) const;

    /**
     * @brief Get a PackLocalEntry for a given hashed path
     *
     * @param key Hash of the relative path used as a key by the index
     * @return const PackLocalEntry&
     */
    inline const PackLocalEntry& get_entry(hash_t key) const;

    /**
     * @brief Recursively pack a directory's content.
     * If a "kpakignore" file is present at the root, all files listed in it (relative to the root)  will be skipped.
     * The "kpakignore" file itself will not be packed. The *kpack* utility makes this feature accessible through a
     * command line interface, allowing to pack directories from a script, for example.
     *
     * @param dir_path Path to the directory to pack
     * @param archive_path Path to the output pack file
     * @return true if the directory was found and packed successfully
     * @return false otherwise
     */
    static bool pack_directory(const fs::path& dir_path, const fs::path& archive_path,
                               const kb::log::Channel* log_channel = nullptr);

private:
    fs::path filepath_;
    std::unordered_map<hash_t, PackLocalEntry> index_;
    const kb::log::Channel* log_channel_ = nullptr;
};

inline std::shared_ptr<std::istream> PackFile::get_input_stream(const std::string& path) const
{
    return get_input_stream(get_entry(path));
}

inline const PackLocalEntry& PackFile::get_entry(const std::string& path) const
{
    return get_entry(H_(path));
}

inline const PackLocalEntry& PackFile::get_entry(hash_t key) const
{
    auto findit = index_.find(key);
    K_ASSERT(findit != index_.end(), "Unknown entry.", log_channel_).watch(key);
    return findit->second;
}

} // namespace kb::kfs