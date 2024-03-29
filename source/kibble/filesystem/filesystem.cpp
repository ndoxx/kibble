#include "filesystem/filesystem.h"
#include "assert/assert.h"
#include "filesystem/resource_pack.h"
#include "logger2/logger.h"
#include "string/string.h"

#include <chrono>
#include "fmt/std.h"
#include <regex>

#ifdef __linux__
#include <climits>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#elif _WIN32

#endif

namespace kb::kfs
{

template <typename TP>
std::time_t to_time_t(TP tp)
{
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
    return system_clock::to_time_t(sctp);
}

struct FileSystem::UpathParsingResult
{
    const DirectoryAlias* alias_entry = nullptr;
    std::string path_component;
};

FileSystem::FileSystem(const kb::log::Channel* log_channel) : log_channel_(log_channel)
{
    // Locate binary
    init_self_path();
}

FileSystem::~FileSystem()
{
    for (auto&& [key, entry] : aliases_)
        for (auto* packfile : entry.packfiles)
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
    const char* homebuf;
    if ((homebuf = getenv("HOME")) == NULL)
        homebuf = getpwuid(getuid())->pw_dir;

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!", log_channel_)
        .watch(home_directory);

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
            klog(log_channel_)
                .uid("FileSystem")
                .error("Failed to create config directory at:\n{}", app_settings_directory_);
            return false;
        }
        klog(log_channel_).uid("FileSystem").info("Created application directory at:\n{}", app_settings_directory_);
    }
    else
    {
        klog(log_channel_).uid("FileSystem").info("Detected application directory at:\n{}", app_settings_directory_);
    }

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
    const char* homebuf;
    if ((homebuf = getenv("HOME")) == NULL)
        homebuf = getpwuid(getuid())->pw_dir;

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!", log_channel_)
        .watch(home_directory);

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
            klog(log_channel_)
                .uid("FileSystem")
                .error("Failed to create application data directory at:\n{}", app_data_directory_);
            return false;
        }
        klog(log_channel_).uid("FileSystem").info("Created application directory at:\n{}", app_data_directory_);
    }
    else
    {
        klog(log_channel_).uid("FileSystem").info("Detected application directory at:\n{}", app_data_directory_);
    }

    // Alias the data directory
    if (alias.empty())
        alias = "appdata";
    alias_directory(app_data_directory_, alias);

    return true;
}

fs::path FileSystem::get_app_data_directory(std::string vendor, std::string appname) const
{
    // Strip spaces in provided arguments
    su::strip_spaces(vendor);
    su::strip_spaces(appname);

#ifdef __linux__
    // * Locate home directory
    // First, check the HOME environment variable, and if not set, fallback to getpwuid()
    const char* homebuf;
    if ((homebuf = getenv("HOME")) == NULL)
        homebuf = getpwuid(getuid())->pw_dir;

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!", log_channel_)
        .watch(home_directory);

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
        klog(log_channel_)
            .uid("FileSystem")
            .error(R"(Application data directory does not exist for:
Vendor:   {}
App name: {}
Searched the following paths:
    - {}
    - {}
=> Returning empty path.)",
                   vendor, appname, candidate1, candidate2);
        return "";
    }

#else
#error get_app_data_directory(2) not yet implemented for this platform.
#endif
}

const fs::path& FileSystem::get_settings_directory() const
{
    if (app_settings_directory_.empty())
    {
        klog(log_channel_)
            .uid("FileSystem")
            .warn("Application config directory has not been setup.\nCall setup_config_directory() after FileSystem "
                  "construction.\nAn empty path will be returned.");
    }
    return app_settings_directory_;
}

const fs::path& FileSystem::get_app_data_directory() const
{
    if (app_data_directory_.empty())
    {
        klog(log_channel_)
            .uid("FileSystem")
            .warn("Application data directory has not been setup.\nCall setup_app_data_directory() after FileSystem "
                  "construction.\nAn empty path will be returned.");
    }
    return app_data_directory_;
}

void FileSystem::sync(const fs::path& source, const fs::path& target) const
{
    // Sanity check
    if ((fs::status(target).permissions() & fs::perms::owner_write) == fs::perms::none)
    {
        klog(log_channel_).uid("FileSystem").error("Target access denied");
        return;
    }

    bool dir_op = fs::is_directory(source);

    klog(log_channel_)
        .uid("FileSystem")
        .info(R"(Syncing {}:
source: {}
target: {})",
              (dir_op ? "directory" : "file"), source.string(), target.string());

    std::error_code ec_copy;

    if (dir_op)
    {
        // Copy recursively
        fs::copy(source, target, fs::copy_options::update_existing | fs::copy_options::recursive, ec_copy);

        // Also delete removed files
        std::vector<fs::path> kill_list;
        for (const auto& entry : fs::recursive_directory_iterator(target))
        {
            fs::path rel = fs::relative(entry.path(), target);
            if (!fs::exists(source / rel))
                kill_list.push_back(entry.path());
        }

        std::error_code ec_remove;
        for (const auto& path : kill_list)
        {
            if (fs::exists(path))
            {
                fs::remove_all(path, ec_remove);
                if (ec_remove)
                {
                    klog(log_channel_).uid("FileSystem").error(ec_remove.message());
                }
            }
        }
    }
    else
        fs::copy_file(source, target, fs::copy_options::update_existing, ec_copy);

    if (ec_copy)
    {
        klog(log_channel_).uid("FileSystem").error(ec_copy.message());
    }
}

bool FileSystem::is_older(const std::string& unipath_1, const std::string& unipath_2) const
{
    auto path_1 = regular_path(unipath_1);
    auto path_2 = regular_path(unipath_2);

    K_ASSERT(fs::exists(path_1), "First path does not exist", log_channel_).watch(unipath_1);
    K_ASSERT(fs::exists(path_2), "Second path does not exist", log_channel_).watch(unipath_2);

    std::time_t cftime_1 = to_time_t(fs::last_write_time(path_1));
    std::time_t cftime_2 = to_time_t(fs::last_write_time(path_2));

    return (cftime_2 > cftime_1);
}

bool FileSystem::alias_directory(const fs::path& _dir_path, const std::string& alias)
{
    // Maybe the path was specified with proximate syntax (../) -> make it lexically normal and absolute
    fs::path dir_path(fs::canonical(_dir_path));

    if (!fs::exists(dir_path))
    {
        klog(log_channel_)
            .uid("FileSystem")
            .error("Cannot add directory alias. Directory does not exist:\n{}", dir_path);
        return false;
    }
    K_ASSERT(fs::is_directory(dir_path), "Not a directory.", log_channel_).watch(dir_path);

    hash_t alias_hash = H_(alias);
    auto findit = aliases_.find(alias_hash);
    if (findit != aliases_.end())
    {
        auto& entry = findit->second;
        entry.alias = alias;
        entry.base = dir_path;
        klog(log_channel_).uid("FileSystem").debug("Overloaded directory alias:\n{}:// <=> {}", alias, dir_path);
    }
    else
    {
        klog(log_channel_).uid("FileSystem").debug("Added directory alias:\n{}:// <=> {}", alias, dir_path);
        aliases_.insert({alias_hash, {alias, dir_path, {}}});
    }

    return true;
}

bool FileSystem::alias_packfile(const fs::path& pack_path, const std::string& alias)
{
    if (!fs::exists(pack_path))
    {
        klog(log_channel_).uid("FileSystem").error("Cannot add pack alias. File does not exist:\n{}", pack_path);
        return false;
    }
    K_ASSERT(fs::is_regular_file(pack_path), "Not a file.", log_channel_).watch(pack_path);

    hash_t alias_hash = H_(alias);
    auto findit = aliases_.find(alias_hash);
    if (findit != aliases_.end())
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

    klog(log_channel_).uid("FileSystem").debug("Added pack alias:\n{}:// <=> {}", alias, pack_path);
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

FileSystem::UpathParsingResult FileSystem::parse_universal_path(const std::string& unipath) const
{
    UpathParsingResult result;

    // Try to regex match an aliasing of the form "alias://path/to/file"
    static const std::regex r_alias("^(.+?)://(.+)");
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

fs::path FileSystem::to_regular_path(const UpathParsingResult& result) const
{
    // If an alias was found, path_component is relative to the aliased base directory
    if (result.alias_entry)
        return fs::absolute((result.alias_entry->base / result.path_component)).lexically_normal();
    // Simply return a canonical path
    return fs::absolute(result.path_component).lexically_normal();
}

IStreamPtr FileSystem::get_input_stream(const std::string& unipath, bool binary) const
{
    klog(log_channel_).uid("FileSystem").debug("Opening stream. Universal path: {}", unipath);

    auto result = parse_universal_path(unipath);
    // If a pack file is aliased at this name, try to find entry in pack
    if (result.alias_entry)
    {
        for (const auto* ppack : result.alias_entry->packfiles)
        {
            auto findit = ppack->find(result.path_component);
            if (findit != ppack->end())
            {
                klog(log_channel_).uid("FileSystem").verbose("source: pack");
                return ppack->get_input_stream(findit->second);
            }
        }
    }

    // If we got here, either the file does not exist at all, or it is in a regular directory
    auto filepath = to_regular_path(result);

    klog(log_channel_).uid("FileSystem").verbose("source: regular file");
    klog(log_channel_).uid("FileSystem").verbose("path:   {}", filepath);

    K_ASSERT(fs::exists(filepath), "File does not exist", log_channel_).watch(unipath);
    K_ASSERT(fs::is_regular_file(filepath), "Not a file", log_channel_).watch(unipath);

    auto mode = std::ios::in;
    if (binary)
        mode |= std::ios::binary;

    std::shared_ptr<std::ifstream> pifs(new std::ifstream(filepath, mode));
    K_ASSERT(pifs->is_open(), "Unable to open input file.", log_channel_).watch(filepath);
    return pifs;
}

void FileSystem::init_self_path()
{
    fs::path self_path;
#ifdef __linux__
    char buff[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
    K_ASSERT(len != -1, "Cannot read self path using readlink.", log_channel_).watch(len);

    if (len != -1)
        buff[len] = '\0';
    self_path = fs::path(buff);
#else
#error init_self_path() not yet implemented for this platform.
#endif

    self_directory_ = fs::canonical(self_path.parent_path());
    K_ASSERT(fs::exists(self_directory_), "Self directory does not exist, that should not be possible!", log_channel_)
        .watch(self_directory_);
}

} // namespace kb::kfs