#include "filesystem/filesystem.h"
#include "assert/assert.h"
#include "filesystem/resource_pack.h"
#include "logger/logger.h"
#include "string/string.h"

#include <chrono>
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

template <typename TP>
std::time_t to_time_t(TP tp)
{
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
    return system_clock::to_time_t(sctp);
}

struct FileSystem::UpathParsingResult
{
    const DirectoryAlias *alias_entry = nullptr;
    std::string path_component;
};

FileSystem::FileSystem()
{
    // Locate binary
    init_self_path();
}

FileSystem::~FileSystem()
{
    for (auto &&[key, entry] : aliases_)
        for (auto *packfile : entry.packfiles)
            delete packfile;
}

bool FileSystem::setup_settings_directory(std::string vendor, std::string appname, std::string alias)
{
    // Strip spaces in provided arguments
    su::strip_spaces(vendor);
    su::strip_spaces(appname);

#ifdef __linux__
    // * Locate home directory
    // First, check the HOME environment variable, and if not set, fallback to getpwuid()
    const char *homebuf;
    if ((homebuf = getenv("HOME")) == NULL)
        homebuf = getpwuid(getuid())->pw_dir;

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!");

    // Check if ~/.config/<vendor>/<appname> is applicable, if not, fallback to
    // ~/.<vendor>/<appname>/config
    if (fs::exists(home_directory / ".config"))
        app_settings_directory_ = home_directory / ".config" / vendor / appname;
    else
        app_settings_directory_ = home_directory / su::concat('.', vendor) / appname / "config";
#else
#error setup_config_directory() not yet implemented for this platform.
#endif

    // If directories do not exist, create them
    if (!fs::exists(app_settings_directory_))
    {
        if (!fs::create_directories(app_settings_directory_))
        {
            KLOGE("ios") << "Failed to create config directory at:" << std::endl;
            KLOGI << KS_PATH_ << app_settings_directory_ << std::endl;
            return false;
        }
        KLOGN("ios") << "Created application directory at:" << std::endl;
    }
    else
    {
        KLOGN("ios") << "Detected application directory at:" << std::endl;
    }
    KLOGI << KS_PATH_ << app_settings_directory_ << std::endl;

    // Alias the config directory
    if (alias.empty())
        alias = "config";
    alias_directory(app_settings_directory_, alias);

    return true;
}

bool FileSystem::setup_app_data_directory(std::string vendor, std::string appname, std::string alias)
{
    // Strip spaces in provided arguments
    su::strip_spaces(vendor);
    su::strip_spaces(appname);

#ifdef __linux__
    // * Locate home directory
    // First, check the HOME environment variable, and if not set, fallback to getpwuid()
    const char *homebuf;
    if ((homebuf = getenv("HOME")) == NULL)
        homebuf = getpwuid(getuid())->pw_dir;

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!");

    // Check if ~/.local/share/<vendor>/<appname> is applicable, if not, fallback to
    // ~/.<vendor>/<appname>/appdata
    if (fs::exists(home_directory / ".local/share"))
        app_data_directory_ = home_directory / ".local/share" / vendor / appname;
    else
        app_data_directory_ = home_directory / su::concat('.', vendor) / appname / "appdata";
#else
#error setup_app_data_directory() not yet implemented for this platform.
#endif

    // If directories do not exist, create them
    if (!fs::exists(app_data_directory_))
    {
        if (!fs::create_directories(app_data_directory_))
        {
            KLOGE("ios") << "Failed to create application data directory at:" << std::endl;
            KLOGI << KS_PATH_ << app_data_directory_ << std::endl;
            return false;
        }
        KLOGN("ios") << "Created application directory at:" << std::endl;
    }
    else
    {
        KLOGN("ios") << "Detected application directory at:" << std::endl;
    }
    KLOGI << KS_PATH_ << app_data_directory_ << std::endl;

    // Alias the data directory
    if (alias.empty())
        alias = "appdata";
    alias_directory(app_data_directory_, alias);

    return true;
}

fs::path FileSystem::get_app_data_directory(std::string vendor, std::string appname)
{
    // Strip spaces in provided arguments
    su::strip_spaces(vendor);
    su::strip_spaces(appname);

#ifdef __linux__
    // * Locate home directory
    // First, check the HOME environment variable, and if not set, fallback to getpwuid()
    const char *homebuf;
    if ((homebuf = getenv("HOME")) == NULL)
        homebuf = getpwuid(getuid())->pw_dir;

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!");

    // Check if ~/.local/share/<vendor>/<appname> exists, if not, fallback to
    // ~/.<vendor>/<appname>/appdata
    auto candidate1 = home_directory / ".local/share" / vendor / appname;
    auto candidate2 = home_directory / su::concat('.', vendor) / appname / "appdata";

    if (fs::exists(candidate1))
        return candidate1;
    else if (fs::exists(candidate2))
        return candidate2;
    else
    {
        KLOGE("ios") << "Application data directory does not exist for:\n";
        KLOGE("ios") << "Vendor: " << vendor << '\n';
        KLOGE("ios") << "App name: " << appname << '\n';
        KLOGE("ios") << "Searched the following paths:\n";
        KLOGE("ios") << KS_PATH_ << candidate1 << '\n';
        KLOGE("ios") << KS_PATH_ << candidate2 << KC_ << '\n';
        KLOGE("ios") << "Returning empty path." << std::endl;
        return "";
    }

#else
#error get_app_data_directory(2) not yet implemented for this platform.
#endif
}

const fs::path &FileSystem::get_settings_directory()
{
    if (app_settings_directory_.empty())
    {
        KLOGW("ios") << "Application config directory has not been setup." << std::endl;
        KLOGI << "Call setup_config_directory() after FileSystem construction." << std::endl;
        KLOGI << "An empty path will be returned." << std::endl;
    }
    return app_settings_directory_;
}

const fs::path &FileSystem::get_app_data_directory()
{
    if (app_data_directory_.empty())
    {
        KLOGW("ios") << "Application data directory has not been setup." << std::endl;
        KLOGI << "Call setup_app_data_directory() after FileSystem construction." << std::endl;
        KLOGI << "An empty path will be returned." << std::endl;
    }
    return app_data_directory_;
}

bool FileSystem::is_older(const std::string &unipath_1, const std::string &unipath_2)
{
    auto path_1 = regular_path(unipath_1);
    auto path_2 = regular_path(unipath_2);

    K_ASSERT(fs::exists(path_1), "First path does not exist.");
    K_ASSERT(fs::exists(path_2), "Second path does not exist.");

    std::time_t cftime_1 = to_time_t(fs::last_write_time(path_1));
    std::time_t cftime_2 = to_time_t(fs::last_write_time(path_2));

    return (cftime_2 > cftime_1);
}

bool FileSystem::alias_directory(const fs::path &_dir_path, const std::string &alias)
{
    // Maybe the path was specified with proximate syntax (../) -> make it lexically normal and absolute
    fs::path dir_path(fs::canonical(_dir_path));

    if (!fs::exists(dir_path))
    {
        KLOGE("ios") << "Cannot add directory alias. Directory does not exist:" << std::endl;
        KLOGI << KS_PATH_ << dir_path << std::endl;
        return false;
    }
    K_ASSERT(fs::is_directory(dir_path), "Not a directory.");

    hash_t alias_hash = H_(alias);
    auto findit = aliases_.find(alias_hash);
    if (findit != aliases_.end())
    {
        auto &entry = findit->second;
        entry.alias = alias;
        entry.base = dir_path;
        KLOG("ios", 0) << "Overloaded directory alias:" << std::endl;
    }
    else
    {
        KLOG("ios", 0) << "Added directory alias:" << std::endl;
        aliases_.insert({alias_hash, {alias, dir_path, {}}});
    }

    KLOGI << alias << "://  <=>  " << KS_PATH_ << dir_path << std::endl;
    return true;
}

bool FileSystem::alias_packfile(const fs::path &pack_path, const std::string &alias)
{
    if (!fs::exists(pack_path))
    {
        KLOGE("ios") << "Cannot add pack alias. File does not exist:" << std::endl;
        KLOGI << KS_PATH_ << pack_path << std::endl;
        return false;
    }
    K_ASSERT(fs::is_regular_file(pack_path), "Not a file.");

    hash_t alias_hash = H_(alias);
    auto findit = aliases_.find(alias_hash);
    if (findit != aliases_.end())
    {
        auto &entry = findit->second;
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
    KLOGI << alias << "://  <=> " << KS_PATH_ << '@' << pack_path << std::endl;
    return true;
}

fs::path FileSystem::regular_path(const std::string &unipath) const
{
    return to_regular_path(parse_universal_path(unipath));
}

std::string FileSystem::make_universal(const fs::path &path, hash_t base_alias_hash) const
{
    // Make path relative to the aliased directory
    const auto &alias_entry = get_alias_entry(base_alias_hash);
    auto rel_path = fs::relative(path, alias_entry.base);
    return su::concat(alias_entry.alias, "://", rel_path.string());
}

FileSystem::UpathParsingResult FileSystem::parse_universal_path(const std::string &unipath) const
{
    UpathParsingResult result;

    // Try to regex match an aliasing of the form "alias://path/to/file"
    std::smatch match;
    if (std::regex_search(unipath, match, r_alias))
    {
        // Unalias
        hash_t alias_hash = H_(match[1].str());
        auto findit = aliases_.find(alias_hash);
        if (findit != aliases_.end())
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

fs::path FileSystem::to_regular_path(const UpathParsingResult &result) const
{
    // If an alias was found, path_component is relative to the aliased base directory
    if (result.alias_entry)
        return fs::absolute((result.alias_entry->base / result.path_component)).lexically_normal();
    // Simply return a canonical path
    return fs::absolute(result.path_component).lexically_normal();
}

IStreamPtr FileSystem::get_input_stream(const std::string &unipath, bool binary) const
{
    KLOG("ios", 0) << "Opening stream:" << std::endl;
    KLOGI << "universal: " << KS_PATH_ << unipath << std::endl;

    auto result = parse_universal_path(unipath);
    // If a pack file is aliased at this name, try to find entry in pack
    if (result.alias_entry)
    {
        for (const auto *ppack : result.alias_entry->packfiles)
        {
            auto findit = ppack->find(result.path_component);
            if (findit != ppack->end())
            {
                KLOGI << "source: " << KS_INST_ << "pack" << std::endl;
                return ppack->get_input_stream(findit->second);
            }
        }
    }

    // If we got here, either the file does not exist at all, or it is in a regular directory
    auto filepath = to_regular_path(result);

    KLOGI << "source:    " << KS_INST_ << "regular file" << std::endl;
    KLOGI << "path:      " << KS_PATH_ << filepath << std::endl;

    K_ASSERT(fs::exists(filepath), "File does not exist.");
    K_ASSERT(fs::is_regular_file(filepath), "Not a file.");

    auto mode = std::ios::in;
    if (binary)
        mode |= std::ios::binary;

    std::shared_ptr<std::ifstream> pifs(new std::ifstream(filepath, mode));
    K_ASSERT(pifs->is_open(), "Unable to open input file.");
    return pifs;
}

void FileSystem::init_self_path()
{
    fs::path self_path;
#ifdef __linux__
    char buff[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
    K_ASSERT(len != -1, "Cannot read self path using readlink.");

    if (len != -1)
        buff[len] = '\0';
    self_path = fs::path(buff);
#else
#error init_self_path() not yet implemented for this platform.
#endif

    self_directory_ = fs::canonical(self_path.parent_path());
    K_ASSERT(fs::exists(self_directory_), "Self directory does not exist, that should not be possible!");
}

} // namespace kfs
} // namespace kb