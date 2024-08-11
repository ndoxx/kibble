#include "filesystem/filesystem.h"
#include "assert/assert.h"
#include "filesystem/resource_pack.h"
#include "logger/logger.h"
#include "string/string.h"

#include <chrono>
#include <fstream>
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
    const AliasEntry* alias_entry = nullptr;
    std::string path_component;
};

FileSystem::FileSystem(const kb::log::Channel* log_channel) : log_channel_(log_channel)
{
    // Locate binary
    init_self_path();
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
    {
        homebuf = getpwuid(getuid())->pw_dir;
    }

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!\n  -> {}",
             home_directory.string());

    // Check if ~/.config/<vendor>/<appname> is applicable, if not, fallback to
    // ~/.<vendor>/<appname>/config
    if (fs::exists(home_directory / ".config"))
    {
        app_settings_directory_ = home_directory / ".config" / vendor / appname;
    }
    else
    {
        app_settings_directory_ = home_directory / fmt::format(".{}", vendor) / appname / "config";
    }
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
                .error("Failed to create config directory at:\n{}", app_settings_directory_.c_str());
            return false;
        }
        klog(log_channel_)
            .uid("FileSystem")
            .info("Created application directory at:\n{}", app_settings_directory_.c_str());
    }
    else
    {
        klog(log_channel_)
            .uid("FileSystem")
            .info("Detected application directory at:\n{}", app_settings_directory_.c_str());
    }

    // Alias the config directory
    if (alias.empty())
    {
        alias = "config";
    }
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
    {
        homebuf = getpwuid(getuid())->pw_dir;
    }

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!\n  -> {}",
             home_directory.string());

    // Check if ~/.local/share/<vendor>/<appname> is applicable, if not, fallback to
    // ~/.<vendor>/<appname>/appdata
    if (fs::exists(home_directory / ".local/share"))
    {
        app_data_directory_ = home_directory / ".local/share" / vendor / appname;
    }
    else
    {
        app_data_directory_ = home_directory / fmt::format(".{}", vendor) / appname / "appdata";
    }
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
                .error("Failed to create application data directory at:\n{}", app_data_directory_.c_str());
            return false;
        }
        klog(log_channel_).uid("FileSystem").info("Created application directory at:\n{}", app_data_directory_.c_str());
    }
    else
    {
        klog(log_channel_)
            .uid("FileSystem")
            .info("Detected application directory at:\n{}", app_data_directory_.c_str());
    }

    // Alias the data directory
    if (alias.empty())
    {
        alias = "appdata";
    }
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
    {
        homebuf = getpwuid(getuid())->pw_dir;
    }

    fs::path home_directory = fs::canonical(homebuf);
    K_ASSERT(fs::exists(home_directory), "Home directory does not exist, that should not be possible!\n  -> {}",
             home_directory.string());

    // Check if ~/.local/share/<vendor>/<appname> exists, if not, fallback to
    // ~/.<vendor>/<appname>/appdata
    auto candidate1 = home_directory / ".local/share" / vendor / appname;
    auto candidate2 = home_directory / fmt::format(".{}", vendor) / appname / "appdata";

    if (fs::exists(candidate1))
    {
        return candidate1;
    }
    else if (fs::exists(candidate2))
    {
        return candidate2;
    }
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
                   vendor, appname, candidate1.c_str(), candidate2.c_str());
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

    if (dir_op)
    {
        sync_directory(source, target);
    }
    else
    {
        sync_file(source, target);
    }
}

void FileSystem::sync_file(const fs::path& source, const fs::path& target) const
{
    std::error_code ec;

    if (!fs::exists(target) || fs::last_write_time(source) > fs::last_write_time(target))
    {
        fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            klog(log_channel_).uid("FileSystem").error("File copy error: {}", ec.message());
        }
        else
        {
            klog(log_channel_).uid("FileSystem").verbose("Updated file: {}", target.string());
        }
    }
}

void FileSystem::sync_directory(const fs::path& source, const fs::path& target) const
{
    // Create target directory if it doesn't exist
    if (!fs::exists(target))
    {
        fs::create_directories(target);
    }

    // Sync files and subdirectories recursively
    for (const auto& entry : fs::directory_iterator(source))
    {
        const auto& source_path = entry.path();
        const auto target_path = target / source_path.filename();

        if (fs::is_directory(source_path))
        {
            sync_directory(source_path, target_path);
        }
        else
        {
            sync_file(source_path, target_path);
        }
    }

    // Remove files/directories in target that don't exist in source
    for (const auto& entry : fs::directory_iterator(target))
    {
        const auto& target_path = entry.path();
        const auto source_path = source / target_path.filename();

        if (!fs::exists(source_path))
        {
            std::error_code ec;
            fs::remove_all(target_path, ec);
            if (ec)
            {
                klog(log_channel_).uid("FileSystem").error("Remove error: {}", ec.message());
            }
            else
            {
                klog(log_channel_).uid("FileSystem").verbose("Removed: {}", target_path.string());
            }
        }
    }
}

bool FileSystem::is_older(const std::string& unipath_1, const std::string& unipath_2) const
{
    auto path_1 = regular_path(unipath_1);
    auto path_2 = regular_path(unipath_2);

    K_ASSERT(fs::exists(path_1), "First path does not exist: {}", unipath_1);
    K_ASSERT(fs::exists(path_2), "Second path does not exist: {}", unipath_2);

    std::time_t cftime_1 = to_time_t(fs::last_write_time(path_1));
    std::time_t cftime_2 = to_time_t(fs::last_write_time(path_2));

    return (cftime_2 > cftime_1);
}

bool FileSystem::alias_directory(const fs::path& _dir_path, const std::string& alias)
{
    // Maybe the path was specified with proximate syntax (../) -> make it lexically normal and absolute
    fs::path dir_path(fs::weakly_canonical(_dir_path));

    if (!fs::exists(dir_path))
    {
        klog(log_channel_)
            .uid("FileSystem")
            .error("Cannot add directory alias. Directory does not exist:\n{}", dir_path.c_str());
        return false;
    }
    K_ASSERT(fs::is_directory(dir_path), "Not a directory: {}", dir_path.string());

    hash_t alias_hash = H_(alias);

    if (auto findit = aliases_.find(alias_hash); findit != aliases_.end())
    {
        findit->second.base = dir_path;
    }
    else
    {
        aliases_[alias_hash] = AliasEntry{.alias = alias, .base = dir_path, .pak = nullptr};
    }
    klog(log_channel_).uid("FileSystem").debug("Added directory alias:\n{}:// <=> {}", alias, dir_path.c_str());

    return true;
}

bool FileSystem::alias_packfile(std::shared_ptr<std::istream> pack_stream, const std::string& alias)
{
    if (pack_stream == nullptr || !(*pack_stream))
    {
        klog(log_channel_).uid("FileSystem").error("Cannot add pack alias. Invalid stream.");
        return false;
    }

    hash_t alias_hash = H_(alias);
    if (auto findit = aliases_.find(alias_hash); findit != aliases_.end())
    {
        findit->second.pak = std::make_unique<PackFile>(pack_stream);
    }
    else
    {
        aliases_[alias_hash] = AliasEntry{.alias = alias, .base = "", .pak = std::make_unique<PackFile>(pack_stream)};
    }

    klog(log_channel_).uid("FileSystem").debug("Added pack alias:\n{}://", alias);

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
    return fmt::format("{}://{}", alias_entry.alias, rel_path.c_str());
}

const FileSystem::AliasEntry& FileSystem::get_alias_entry(hash_t alias_hash) const
{
    auto findit = aliases_.find(alias_hash);
    K_ASSERT(findit != aliases_.end(), "Unknown alias: {}", alias_hash);
    return findit->second;
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
        if (auto dir_it = aliases_.find(alias_hash); dir_it != aliases_.end())
        {
            result.alias_entry = &dir_it->second;
            result.path_component = match[2].str();
        }
    }
    // A regular path was submitted
    else
    {
        result.path_component = unipath;
    }

    return result;
}

fs::path FileSystem::to_regular_path(const UpathParsingResult& result) const
{
    // If an alias was found, path_component is relative to the aliased base directory
    if (result.alias_entry)
    {
        return fs::absolute((result.alias_entry->base / result.path_component)).lexically_normal();
    }
    // Simply return a canonical path
    return fs::absolute(result.path_component).lexically_normal();
}

IStreamPtr FileSystem::get_input_stream(const std::string& unipath, bool binary) const
{
    klog(log_channel_).uid("FileSystem").debug("Opening stream. Universal path: {}", unipath);

    auto result = parse_universal_path(unipath);
    // If a pack file is aliased at this name, try to find entry in pack
    if (result.alias_entry && result.alias_entry->pak != nullptr)
    {
        if (auto pstream = result.alias_entry->pak->get_input_stream(H_(result.path_component)); pstream)
        {
            klog(log_channel_).uid("FileSystem").verbose("source: pack");
            return pstream;
        }
    }

    // If we got here, either the file does not exist at all, or it is in a regular directory
    auto filepath = to_regular_path(result);

    klog(log_channel_).uid("FileSystem").verbose("source: regular file");
    klog(log_channel_).uid("FileSystem").verbose("path:   {}", filepath.c_str());

    K_ASSERT(fs::exists(filepath), "File does not exist: {}", unipath);
    K_ASSERT(fs::is_regular_file(filepath), "Not a file: {}", unipath);

    auto mode = std::ios::in;
    if (binary)
    {
        mode |= std::ios::binary;
    }

    std::shared_ptr<std::ifstream> pifs(new std::ifstream(filepath, mode));
    K_ASSERT(pifs->is_open(), "Unable to open input file: {}", filepath.string());
    return pifs;
}

void FileSystem::init_self_path()
{
    fs::path self_path;
#ifdef __linux__
    char buff[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
    K_ASSERT(len != -1, "Cannot read self path using readlink. Buf len: {}", len);

    if (len != -1)
    {
        buff[len] = '\0';
    }
    self_path = fs::path(buff);
#else
#error init_self_path() not yet implemented for this platform.
#endif

    self_directory_ = fs::canonical(self_path.parent_path());
    K_ASSERT(fs::exists(self_directory_), "Self directory does not exist, that should not be possible!\n  -> {}",
             self_directory_.string());
}

} // namespace kb::kfs