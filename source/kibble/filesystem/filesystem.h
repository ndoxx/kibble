#pragma once
/* TODO:
 * - Return stream to file inside packed resource file
 * - Packs can override a directory alias
 * - Handle OS-dep preferred user settings dir & other preferred dirs if any
 */

#include <filesystem>
#include <fstream>
#include <istream>
#include <map>
#include <memory>
#include <ostream>
#include <type_traits>
#include <vector>

#include "../hash/hashstr.h"

namespace fs = std::filesystem;
namespace kb
{
namespace kfs
{

using IStreamPtr = std::shared_ptr<std::istream>;
using OStreamPtr = std::shared_ptr<std::ostream>;

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
    inline const fs::path& get_aliased_directory(const std::string& alias) const
    {
        return get_aliased_directory(H_(alias));
    }
    // Return the absolute lexically normal path to this binary's parent directory
    inline const fs::path& get_self_directory() const { return self_directory_; }

    // Return an input stream to a file
    IStreamPtr input_stream(const std::string& unipath, bool binary = true) const;
    // Return an output stream to a file
    OStreamPtr output_stream(const std::string& unipath, bool binary = true) const;
    // Return content of a file as a vector of chosen integral type
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    std::vector<T> get_file_as_vector(const std::string& unipath) const;
    // Return content of a file as a string
    inline std::string get_file_as_string(const std::string& unipath) const;

    // Helper func to compute the hash of a file extension
    static inline hash_t extension_hash(const fs::path& filepath) { return H_(filepath.extension().string()); }

private:
    static fs::path retrieve_self_path();

private:
    fs::path self_directory_;
    std::map<hash_t, fs::path> aliases_;
};

template <typename T, typename> std::vector<T> FileSystem::get_file_as_vector(const std::string& unipath) const
{
    auto ifs = input_stream(unipath);
    return std::vector<T>(std::istream_iterator<T>(*ifs), std::istream_iterator<T>());
}

inline std::string FileSystem::get_file_as_string(const std::string& unipath) const
{
    auto ifs = input_stream(unipath);
    return std::string((std::istreambuf_iterator<char>(*ifs)), std::istreambuf_iterator<char>());
}

} // namespace kfs
} // namespace kb