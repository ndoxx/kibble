#include "filesystem/filesystem.h"
#include "assert/assert.h"
#include "filesystem/resource_pack.h"
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

struct UpathParsingResult
{
    const DirectoryAlias* alias_entry = nullptr;
    std::string path_component;
};

FileSystem::FileSystem()
{
    // Localize binary
    self_directory_ = fs::canonical(retrieve_self_path().parent_path());
    K_ASSERT(fs::exists(self_directory_), "Self directory does not exist, that should not be possible!");
}

FileSystem::~FileSystem()
{
    for(auto&& [key, entry] : aliases_)
        for(auto* packfile : entry.packfiles)
            delete packfile;
}

bool FileSystem::alias_directory(const fs::path& _dir_path, const std::string& alias)
{
    // Maybe the path was specified with proximate syntax (../) -> make it lexically normal and absolute
    fs::path dir_path(fs::canonical(_dir_path));

    if(!fs::exists(dir_path))
    {
        KLOGE("ios") << "Cannot add directory alias. Directory does not exist:" << std::endl;
        KLOGI << WCC('p') << dir_path << std::endl;
        return false;
    }
    K_ASSERT(fs::is_directory(dir_path), "Not a directory.");

    hash_t alias_hash = H_(alias);
    auto findit = aliases_.find(alias_hash);
    if(findit != aliases_.end())
    {
        auto& entry = findit->second;
        entry.alias = alias;
        entry.base = dir_path;
        KLOG("ios", 0) << "Overloaded directory alias:" << std::endl;
    }
    else
    {
        KLOG("ios", 0) << "Added directory alias:" << std::endl;
        aliases_.insert({alias_hash, {alias, dir_path, {}}});
    }

    KLOGI << alias << "://  <=>  " << WCC('p') << dir_path << std::endl;
    return true;
}

bool FileSystem::alias_packfile(const fs::path& pack_path, const std::string& alias)
{
    if(!fs::exists(pack_path))
    {
        KLOGE("ios") << "Cannot add pack alias. File does not exist:" << std::endl;
        KLOGI << WCC('p') << pack_path << std::endl;
        return false;
    }
    K_ASSERT(fs::is_regular_file(pack_path), "Not a file.");

    hash_t alias_hash = H_(alias);
    auto findit = aliases_.find(alias_hash);
    if(findit != aliases_.end())
    {
        auto& entry = findit->second;
        entry.packfiles.push_back(new PackFile(pack_path));
    }
    else
    {
        // By default, physical directory has the same (stem) name as the pack and is located in the
        // same parent directory
        // e.g. parent/resources.kpak <=> parent/resources
        aliases_.insert({alias_hash, {alias, pack_path.parent_path() / pack_path.stem(), {new PackFile(pack_path)}}});
    }

    KLOG("ios", 0) << "Added pack alias:" << std::endl;
    KLOGI << alias << "://  <=> " << WCC('p') << '@' << pack_path << std::endl;
    return true;
}

fs::path FileSystem::regular_path(const std::string& unipath) const
{
    return to_regular_path(parse_universal_path(unipath));
}

std::string FileSystem::make_universal(const fs::path& path, hash_t base_alias_hash) const
{
    // Make path relative to the aliased directory
    const auto& alias_entry = get_alias_entry(base_alias_hash);
    auto rel_path = fs::relative(path, alias_entry.base);
    return su::concat(alias_entry.alias, "://", rel_path.string());
}

UpathParsingResult FileSystem::parse_universal_path(const std::string& unipath) const
{
    UpathParsingResult result;

    // Try to regex match an aliasing of the form "alias://path/to/file"
    std::smatch match;
    if(std::regex_search(unipath, match, r_alias))
    {
        // Unalias
        hash_t alias_hash = H_(match[1].str());
        auto findit = aliases_.find(alias_hash);
        if(findit != aliases_.end())
        {
            result.alias_entry = &findit->second;
            result.path_component = match[2].str();
        }
    }
    // A regular path was submitted
    else
        result.path_component = unipath;

    return result;
}

fs::path FileSystem::to_regular_path(const UpathParsingResult& result) const
{
    // If an alias was found, path_component is relative to the aliased base directory
    if(result.alias_entry)
        return fs::absolute((result.alias_entry->base / result.path_component)).lexically_normal();
    // Simply return a canonical path
    return fs::absolute(result.path_component).lexically_normal();
}

IStreamPtr FileSystem::get_input_stream(const std::string& unipath, bool binary) const
{
    KLOG("ios", 0) << "Opening stream:" << std::endl;
    KLOGI << "universal: " << WCC('p') << unipath << std::endl;

    auto result = parse_universal_path(unipath);
    // If a pack file is aliased at this name, try to find entry in pack
    if(result.alias_entry)
    {
        for(const auto* ppack : result.alias_entry->packfiles)
        {
            auto findit = ppack->find(result.path_component);
            if(findit != ppack->end())
            {
                KLOGI << "source: " << WCC('i') << "pack" << std::endl;
                return ppack->get_input_stream(findit->second);
            }
        }
    }

    // If we got here, either the file does not exist at all, or it is in a regular directory
    auto filepath = to_regular_path(result);

    KLOGI << "source:    " << WCC('i') << "regular file" << std::endl;
    KLOGI << "path:      " << WCC('p') << filepath << std::endl;

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