#include "filesystem/filesystem.h"
#include "assert/assert.h"
#include "logger/logger.h"
#include "string/string.h"

#include <regex>

#ifdef __linux__
#include <climits>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#elif _WIN32

#endif

namespace kb
{
namespace kfs
{

static const std::regex r_alias("^(.+?)://(.+)");

FileSystem::FileSystem()
{
    // Localize binary
    self_directory_ = fs::canonical(retrieve_self_path().parent_path());
    K_ASSERT(fs::exists(self_directory_), "Self directory does not exist, that should not be possible!");
}

bool FileSystem::add_directory_alias(const fs::path& _dir_path, const std::string& alias)
{
    // Maybe the path was specified with proximate syntax (../) -> make it lexically normal and absolute
    fs::path dir_path(fs::canonical(_dir_path));

    if(!fs::exists(dir_path))
    {
        KLOGE("ios") << "Cannot add directory alias. Directory does not exist:" << std::endl;
        KLOGI << WCC('p') << dir_path << std::endl;
        return false;
    }

    aliases_.insert({H_(alias), {alias, dir_path}});
    KLOG("ios", 0) << "Added directory alias:" << std::endl;
    KLOGI << alias << "://  <=>  " << WCC('p') << dir_path << std::endl;
    return true;
}

const DirectoryAlias& FileSystem::get_aliased_directory_entry(hash_t alias_hash) const
{
    auto findit = aliases_.find(alias_hash);
    K_ASSERT(findit != aliases_.end(), "Unknown alias.");
    return findit->second;
}

fs::path FileSystem::universal_path(const std::string& unipath) const
{
    // Try to regex match an aliasing of the form "alias://path/to/file"
    std::smatch match;
    if(std::regex_search(unipath, match, r_alias))
    {
        // Unalias directory and return a canonical path
        hash_t alias_hash = H_(match[1].str());
        return fs::absolute((get_aliased_directory(alias_hash) / match[2].str())).lexically_normal();
    }
    // Simply return a canonical path
    return fs::absolute(fs::path(unipath)).lexically_normal();
}

std::string FileSystem::make_universal(const fs::path& path, hash_t base_alias_hash) const
{
    // Make path relative to the aliased directory
    const auto& base_alias = get_aliased_directory_entry(base_alias_hash);
    auto rel_path = fs::relative(path, base_alias.path);

    return su::concat(base_alias.alias, "://", rel_path.string());
}

IStreamPtr FileSystem::get_input_stream(const std::string& unipath, bool binary) const
{
    auto filepath = universal_path(unipath);
    K_ASSERT(fs::exists(filepath), "File does not exist.");
    K_ASSERT(fs::is_regular_file(filepath), "Not a file.");

    auto mode = std::ios::in;
    if(binary)
        mode |= std::ios::binary;

    std::shared_ptr<std::ifstream> pifs(new std::ifstream(filepath, mode));
    K_ASSERT(pifs->is_open(), "Unable to open input file.");
    return pifs;
}

fs::path FileSystem::retrieve_self_path()
{
#ifdef __linux__
    char buff[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
    K_ASSERT(len != -1, "Cannot read self path using readlink.");

    if(len != -1)
        buff[len] = '\0';
    return fs::path(buff);
#else
#error get_selfpath() not yet implemented for this platform.
#endif

    return fs::path();
}

} // namespace kfs
} // namespace kb