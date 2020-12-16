#pragma once
/* TODO:
 * - Handle OS-dep preferred user settings dir & other preferred dirs if any
 */

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <ostream>
#include <type_traits>
#include <vector>

#include "../assert/assert.h"
#include "../hash/hashstr.h"

namespace fs = std::filesystem;
namespace kb
{
namespace kfs
{

class PackFile;
struct DirectoryAlias
{
    std::string alias;
    fs::path base;
    std::vector<PackFile*> packfiles;
};

using IStreamPtr = std::shared_ptr<std::istream>;

struct UpathParsingResult;
class FileSystem
{
public:
    FileSystem();
    ~FileSystem();

    // Alias a directory, so a file path relative to this directory can be referenced by
    // an universal path of the form alias://path/to/file
    // Only one physical directory can be aliased by this name. If this function is called
    // for two directories with the same alias, the last one will overload the alias.
    bool alias_directory(const fs::path& dir_path, const std::string& alias);
    // Alias the root of a resource pack file. If a directory alias exists at this name,
    // the file system will behave as if both hierarchies were merged together.
    // Multiple packfiles can be grouped under the same alias. Last aliased pack has
    // the lowest priority.
    bool alias_packfile(const fs::path& pack_path, const std::string& alias);

    // Return an absolute lexically normal path to a file referenced by a universal path string
    fs::path regular_path(const std::string& unipath) const;
    // Return a universal path string given a path and a base directory alias
    std::string make_universal(const fs::path& path, hash_t base_alias_hash) const;
    inline std::string make_universal(const fs::path& path, const std::string& base_alias) const;
    // Return the absolute lexically normal path to the aliased directory
    inline const fs::path& get_aliased_directory(hash_t alias_hash) const;
    inline const fs::path& get_aliased_directory(const std::string& alias) const;
    // Return the absolute lexically normal path to this binary's parent directory
    inline const fs::path& get_self_directory() const { return self_directory_; }

    // Create a config directory for this application, the config directory
    // will be aliased by "config", unless the third parameter is set.
    // Whitespace characters will be stripped from the two first arguments.
    bool setup_config_directory(std::string vendor, std::string appname, std::string alias = "");
    // Get the application config directory
    const fs::path& get_config_directory();

    // Return true if the file/directory pointed to by the first argument is older than
    // the second one. Both paths MUST exist.
    bool is_older(const std::string& unipath_1, const std::string& unipath_2);

    // Return an input stream from a file. Aliased packs (if any) will be searched first.
    IStreamPtr get_input_stream(const std::string& unipath, bool binary = true) const;
    // Return content of a file as a vector of chosen integral type
    template <typename CharT, typename Traits = std::char_traits<CharT>>
    std::vector<CharT> get_file_as_vector(const std::string& unipath) const;
    // Return content of a file as a string
    inline std::string get_file_as_string(const std::string& unipath) const;

private:
    // Return alias entry at that name
    inline const DirectoryAlias& get_alias_entry(hash_t alias_hash) const;
    // Try to match an alias form in a universal path string, return an opaque type
    // containing the parsing results
    UpathParsingResult parse_universal_path(const std::string& unipath) const;
    // Convert parsing results into a physical path
    fs::path to_regular_path(const UpathParsingResult& result) const;
    // OS-dependent method to locate this binary
    void init_self_path();

private:
    fs::path self_directory_;
    fs::path app_config_directory_;
    std::map<hash_t, DirectoryAlias> aliases_;
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

inline const DirectoryAlias& FileSystem::get_alias_entry(hash_t alias_hash) const
{
    auto findit = aliases_.find(alias_hash);
    K_ASSERT(findit != aliases_.end(), "Unknown alias.");
    return findit->second;
}

} // namespace kfs
} // namespace kb