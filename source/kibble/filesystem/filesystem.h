#pragma once
/* TODO:
 * - Return stream to file inside packed resource file
 * - Packs can override a directory alias
 * - Handle OS-dep preferred user settings dir & other preferred dirs if any
*/


#include <filesystem>
#include <map>

#include "../hash/hashstr.h"

namespace fs = std::filesystem;
namespace kb
{
namespace kfs
{

class FileSystem
{
public:
    FileSystem();

    // Add an alias to a directory, so a file path relative to this directory can be referenced by
    // an universal path of the form alias://path/to/file
    bool add_directory_alias(const fs::path& dir_path, const std::string& alias);
    // Return an absolute lexically normal path to a file referenced by a universal path string
    fs::path universal_path(const std::string& unipath) const;
    // Return the absolute lexically normal path to the aliased directory 
    const fs::path& get_aliased_directory(hash_t alias_hash) const;
    inline const fs::path& get_aliased_directory(const std::string& alias) const { return get_aliased_directory(H_(alias)); }
    // Return the absolute lexically normal path to this binary's parent directory
    inline const fs::path& get_self_directory() const { return self_directory_; }

    // Helper func to compute the hash of a file extension
    static inline hash_t extension_hash(const fs::path& filepath) { return H_(filepath.extension().string()); }

private:
    static fs::path retrieve_self_path();

private:
    fs::path self_directory_;
    std::map<hash_t, fs::path> aliases_;
};

} // namespace kfs
} // namespace kb