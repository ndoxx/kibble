#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "kibble/hash/hash.h"
#include "kibble/util/unordered_dense.h"
#include "kibble/filesystem/resource_pack.h"

namespace fs = std::filesystem;

namespace kb::log
{
class Channel;
}

namespace kb::kfs
{

using IStreamPtr = std::unique_ptr<std::istream>;

/**
 * @brief This class provides powerful and varied file interaction capabilities thanks to a variety of concepts like
 * directory aliasing, resource packs and universal paths.
 * - *Resource packs* or *pack files* are archive-like files that can mirror an existing directory hierarchy but are in
 * theory way faster to access than regular directories. For more information, see the PackFile documentation.
 * - *Directory aliases* are strings referencing a directory or the root of a pack file. They are used in conjunction
 * with universal paths to allow for readable and terse path expressions.
 * - *Universal paths* are formatted strings that can be converted to regular paths. They can be either written in the
 * form of a regular path like `path/to/file` or include an alias using the syntax `alias://path/to/file`. In the latter
 * case, the alias can either reference a regular directory or the root of a pack file, and the rest of the path is read
 * relative to the aliased directory / pack. Either way the file system will be able to find the targeted resource.
 *
 * FEATURES:
 * - Transparent directory / resource pack aliasing
 * - Ability for an application to locate its own binary
 * - Application settings directory creation
 *
 */
class FileSystem
{
public:
    FileSystem(const kb::log::Channel* log_channel = nullptr);

    /**
     * @brief Create a directory alias.
     * A file path relative to this directory can be referenced by an universal path
     * of the form alias://path/to/file.
     * Only one physical directory can be aliased by this name. If this function is called
     * for two directories with the same alias, the last one will overload the alias.
     *
     * @param dir_path Path to the directory to alias
     * @param alias Alias name
     * @return success
     */
    bool alias_directory(const fs::path& dir_path, const std::string& alias);

    /**
     * @brief Alias a resource pack file.
     * It allows to treat a pack file as if it were a regular directory.
     * If a directory alias exists at this name, the file system will behave as if both hierarchies were merged
     * together.
     * Because we're using an input stream here, the pack "file" can very well be supplied from a
     * memory buffer via an InputMemoryStream.
     *
     * @param pack_stream Input stream to packfile
     * @param alias Alias name
     * @return success
     */
    bool alias_packfile(IStreamPtr pack_stream, const std::string& alias);

    /**
     * @brief Return an absolute lexically normal path to a file referenced by a universal path string
     *
     * @param unipath The input universal path
     * @return fs::path A native path object in the lexically normal form
     */
    fs::path regular_path(const std::string& unipath) const;

    /**
     * @brief Return a universal path string given a path and a base directory alias
     *
     * @param path Path relative to the alias directory
     * @param base_alias_hash Hash alias name
     * @return std::string A universal path as a string
     */
    std::string make_universal(const fs::path& path, hash_t base_alias_hash) const;

    /**
     * @brief Return a universal path string given a path and a base directory alias
     *
     * @param path Path relative to the alias directory
     * @param base_alias Alias name
     * @return std::string A universal path as a string
     */
    inline std::string make_universal(const fs::path& path, const std::string& base_alias) const;

    /**
     * @brief Return the absolute lexically normal path to the aliased directory
     *
     * @param alias_hash The hashed alias name
     * @return const fs::path& A native absolute path object pointing to the aliased directory
     */
    inline const fs::path& get_aliased_directory(hash_t alias_hash) const;

    /**
     * @brief Return the absolute lexically normal path to the aliased directory
     *
     * @param alias The alias name
     * @return const fs::path& A native absolute path object pointing to the aliased directory
     */
    inline const fs::path& get_aliased_directory(const std::string& alias) const;

    /**
     * @brief Return the absolute lexically normal path to this binary's parent directory
     *
     * @return const fs::path&
     */
    inline const fs::path& get_self_directory() const
    {
        return self_directory_;
    }

    /**
     * @brief Create a config directory for this application.
     * The config directory will be aliased by "config", unless the third parameter is set.
     * Whitespace characters will be stripped from the two first arguments.
     * If the configuration directory already exists, only the aliasing is performed.
     * Under linux systems, this function will try to create the config directory like so:\n
     * `~/.config/<vendor>/<appname>`\n
     * If this is not applicable, it will fall back to this form:\n
     * `~/.<vendor>/<appname>/config`
     *
     * @param vendor The vendor name will be used as a parent directory for the configuration directory of this
     * application. Thus multiple applications can be grouped under the same vendor name
     * @param appname The unique application name used as a configuration directory for this application
     * @param alias Optional alias name to refer to this configuration directory
     * @return true If the directory was created successfully or already exists
     * @return false if there was an error during the creation of the directory
     */
    bool setup_settings_directory(std::string vendor, std::string appname, std::string alias = "");

    /**
     * @brief Create a directory for application data and resources.
     * This directory will be aliased "appdata", unless the third parameter is set.
     * Whitespace characters will be stripped from the two first arguments.
     * If the directory already exists, only the aliasing is performed.
     * Under linux systems, this function will try to create the data directory like so:\n
     * `~/.local/share/<vendor>/<appname>`\n
     * If this is not applicable, it will fall back to this form:\n
     * `~/.<vendor>/<appname>/appdata`
     *
     * @param vendor The vendor name will be used as a parent directory for the data directory of this
     * application. Thus multiple applications can be grouped under the same vendor name
     * @param appname The unique application name used as a data directory for this application
     * @param alias Optional alias name to refer to this data directory
     * @return true If the directory was created successfully or already exists
     * @return false if there was an error during the creation of the directory
     */
    bool setup_app_data_directory(std::string vendor, std::string appname, std::string alias = "");

    /**
     * @brief Get the application config directory.
     * If no config directory exists for this application, an empty path will be returned.
     *
     * @return const fs::path&
     */
    const fs::path& get_settings_directory() const;

    /**
     * @brief Get the application data directory.
     * If no data directory exists for this application, an empty path will be returned.
     *
     * @return const fs::path&
     */
    const fs::path& get_app_data_directory() const;

    /**
     * @brief Get the app data directory of another project.
     *
     * @param vendor
     * @param appname
     * @return fs::path
     */
    fs::path get_app_data_directory(std::string vendor, std::string appname) const;

    /**
     * @brief Synchronize files or directories
     *
     * Only copy files if they don't exist or the source version is newer
     * In directory mode, remove files from target that have been deleted in source
     *
     * @param source The directory / file to copy from
     * @param target The directory / file to copy to
     */
    void sync(const fs::path& source, const fs::path& target) const;

    /**
     * @brief Compare the creation / modification dates of two files.
     * Return true if the file/directory pointed to by the first argument is older than the second one. Both paths MUST
     * exist.
     *
     * @param unipath_1 Universal path to the first file
     * @param unipath_2 Universal path to the second file
     * @return true if the first file is older
     * @return false otherwise
     */
    bool is_older(const std::string& unipath_1, const std::string& unipath_2) const;

    /**
     * @brief Check if a file exists at a given universal path
     *
     * @param unipath A universal path to a potentially existing file
     * @return true if the file exists
     * @return false otherwise
     */
    inline bool exists(const std::string& unipath) const
    {
        return fs::exists(regular_path(unipath));
    }

    /**
     * @brief Check if the extension of a file matches the second argument.
     * The extension dot must be included in the second argument.
     *
     * @param unipath Universal path to the file
     * @param ext Extension string, dot included
     * @return true if the extension matches
     * @return false otherwise
     */
    inline bool check_extension(const std::string& unipath, const std::string& ext) const
    {
        return !regular_path(unipath).extension().compare(ext);
    }

    /**
     * @brief Get the extension string of a file
     *
     * @param unipath Universal path to the file
     * @return std::string The extension string, dot included
     */
    inline std::string extension(const std::string& unipath) const
    {
        return regular_path(unipath).extension();
    }

    /**
     * @brief Return an input stream from a file.
     * Aliased packs (if any) will be searched first. If no file was found in a pack
     * or in a regular directory, a new file will be created in the regular directory
     * matching this universal path and a stream pointer to this file will be returned.
     *
     * @param unipath Universal path to a file
     * @param binary if true, the stream will be in binary mode
     * @return IStreamPtr A unique pointer to an input stream
     */
    IStreamPtr get_input_stream(const std::string& unipath, bool binary = true) const;

    /**
     * @brief Return the content of a file as a vector of the chosen integral type.
     *
     * @tparam CharT Type of character to use
     * @tparam Traits Trait class of the character type
     * @param unipath Universal path to the file
     * @return std::vector<CharT> A vector of characters
     */
    template <typename CharT, typename Traits = std::char_traits<CharT>>
    std::vector<CharT> get_file_as_vector(const std::string& unipath) const;

    /**
     * @brief Return content of a file as a string
     *
     * @param unipath Universal path to the file
     * @return std::string A string with the file content
     */
    inline std::string get_file_as_string(const std::string& unipath) const;

private:
    struct UpathParsingResult;

    struct AliasEntry
    {
        std::string alias;
        fs::path base;
        std::unique_ptr<PackFile> pak;
    };

    // Return alias entry at that name
    const AliasEntry& get_alias_entry(hash_t alias_hash) const;
    // Try to match an alias form in a universal path string, return an opaque type
    // containing the parsing results
    UpathParsingResult parse_universal_path(const std::string& unipath) const;
    // Convert parsing results into a physical path
    fs::path to_regular_path(const UpathParsingResult& result) const;
    // OS-dependent method to locate this binary
    void init_self_path();
    // Helper to sync a single file
    void sync_file(const fs::path& source, const fs::path& target) const;
    // Helper to recursively sync a directory and its content
    void sync_directory(const fs::path& source, const fs::path& target) const;

private:
    fs::path self_directory_;
    fs::path app_settings_directory_;
    fs::path app_data_directory_;
    ankerl::unordered_dense::map<hash_t, AliasEntry> aliases_;
    const kb::log::Channel* log_channel_ = nullptr;
};

inline const fs::path& FileSystem::get_aliased_directory(hash_t alias_hash) const
{
    return get_alias_entry(alias_hash).base;
}

inline const fs::path& FileSystem::get_aliased_directory(const std::string& alias) const
{
    return get_aliased_directory(H_(alias));
}

inline std::string FileSystem::make_universal(const fs::path& path, const std::string& base_alias) const
{
    return make_universal(path, H_(base_alias));
}

template <typename CharT, typename>
std::vector<CharT> FileSystem::get_file_as_vector(const std::string& unipath) const
{
    auto pis = get_input_stream(unipath);
    return std::vector<CharT>(std::istreambuf_iterator<CharT>(*pis), std::istreambuf_iterator<CharT>());
}

inline std::string FileSystem::get_file_as_string(const std::string& unipath) const
{
    auto pis = get_input_stream(unipath);
    return std::string((std::istreambuf_iterator<char>(*pis)), std::istreambuf_iterator<char>());
}

} // namespace kb::kfs