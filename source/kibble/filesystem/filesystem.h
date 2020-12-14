#pragma once
/* TODO:
 * - Return stream to file inside packed resource file
 * - Packs can override a directory alias
 * - Handle OS-dep preferred user settings dir & other preferred dirs if any
 */

#include <filesystem>
#include <fstream>
#include <map>
#include <ostream>
#include <type_traits>
#include <vector>
#include <memory>

#include "../hash/hashstr.h"

namespace fs = std::filesystem;
namespace kb
{
namespace kfs
{

struct DirectoryAlias
{
    std::string alias;
    fs::path path;
};

using IStreamPtr = std::shared_ptr<std::istream>;

class FileSystem
{
public:
    FileSystem();

    // Add an alias to a directory, so a file path relative to this directory can be referenced by
    // an universal path of the form alias://path/to/file
    bool add_directory_alias(const fs::path& dir_path, const std::string& alias);
    // Return an absolute lexically normal path to a file referenced by a universal path string
    fs::path universal_path(const std::string& unipath) const;
    // Return a universal path string given a path and a base directory alias
    std::string make_universal(const fs::path& path, hash_t base_alias_hash) const;
    inline std::string make_universal(const fs::path& path, const std::string& base_alias) const;
    // Return the absolute lexically normal path to the aliased directory
    inline const fs::path& get_aliased_directory(hash_t alias_hash) const;
    inline const fs::path& get_aliased_directory(const std::string& alias) const;
    // Return the absolute lexically normal path to this binary's parent directory
    inline const fs::path& get_self_directory() const { return self_directory_; }

    // Return an input stream to a file
    IStreamPtr get_input_stream(const std::string& unipath, bool binary = true) const;
    // Return content of a file as a vector of chosen integral type
    template <typename CharT, typename Traits = std::char_traits<CharT>>
    std::vector<CharT> get_file_as_vector(const std::string& unipath) const;
    // Return content of a file as a string
    inline std::string get_file_as_string(const std::string& unipath) const;

    // Helper func to compute the hash of a file extension
    static inline hash_t extension_hash(const fs::path& filepath) { return H_(filepath.extension().string()); }

private:
    const DirectoryAlias& get_aliased_directory_entry(hash_t alias_hash) const;

    static fs::path retrieve_self_path();

private:
    fs::path self_directory_;
    std::map<hash_t, DirectoryAlias> aliases_;
};

inline const fs::path& FileSystem::get_aliased_directory(hash_t alias_hash) const
{
    return get_aliased_directory_entry(alias_hash).path;
}

inline const fs::path& FileSystem::get_aliased_directory(const std::string& alias) const
{
    return get_aliased_directory(H_(alias));
}

inline std::string FileSystem::make_universal(const fs::path& path, const std::string& base_alias) const
{
    return make_universal(path, H_(base_alias));
}

template <typename CharT, typename> std::vector<CharT> FileSystem::get_file_as_vector(const std::string& unipath) const
{
    auto pis = get_input_stream(unipath);
    return std::vector<CharT>(std::istreambuf_iterator<CharT>(*pis), std::istreambuf_iterator<CharT>());
}

inline std::string FileSystem::get_file_as_string(const std::string& unipath) const
{
    auto pis = get_input_stream(unipath);
    return std::string((std::istreambuf_iterator<char>(*pis)), std::istreambuf_iterator<char>());
}

} // namespace kfs
} // namespace kb