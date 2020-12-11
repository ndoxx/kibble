#include "filesystem/filesystem.h"
#include "assert/assert.h"
#include "logger/logger.h"

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
    self_directory_ = fs::absolute(retrieve_self_path().parent_path()).lexically_normal();
    K_ASSERT(fs::exists(self_directory_), "Self directory does not exist, that should not be possible!");
}

bool FileSystem::add_directory_alias(const fs::path& _dir_path, const std::string& alias)
{
    // Maybe the path was specified with proximate syntax (../) -> make it normal
    fs::path dir_path(_dir_path.lexically_normal());
    // Make it an absolute path
    if(!dir_path.is_absolute())
        dir_path = fs::absolute(dir_path);

    if(!fs::exists(dir_path))
    {
        KLOGE("ios") << "Cannot add directory alias. Directory does not exist:" << std::endl;
        KLOGI << WCC('p') << dir_path << std::endl;
        return false;
    }

    aliases_.insert({H_(alias), dir_path});
    KLOG("ios", 0) << "Added directory alias:" << std::endl;
    KLOGI << alias << "://  <=>  " << WCC('p') << dir_path << '/' << std::endl;
    return true;
}

const fs::path& FileSystem::get_aliased_directory(hash_t alias_hash) const
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
    	// Unalias directory and return a normal absolute path
    	hash_t alias_hash = H_(match[1].str());
    	return fs::absolute((get_aliased_directory(alias_hash) / match[2].str()).lexically_normal());
    }
    // Simply return a normal absolute path
    return fs::absolute(fs::path(unipath).lexically_normal());
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