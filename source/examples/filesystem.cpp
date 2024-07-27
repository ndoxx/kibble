#include "filesystem/filesystem.h"
#include "filesystem/resource_pack.h"
#include "logger/formatters/vscode_terminal_formatter.h"
#include "logger/logger.h"
#include "logger/sinks/console_sink.h"
#include "math/color_table.h"

#include "fmt/std.h"

namespace fs = std::filesystem;

using namespace kb;
using namespace kb::log;

void list_dir(const fs::path& path, const Channel& chan)
{
    for (const auto& entry : fs::recursive_directory_iterator(path))
    {
        bool is_dir = entry.is_directory();
        klog(chan).info("- {}: {}", (is_dir ? 'd' : 'f'), entry.path().string());
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan(Severity::Verbose, "kibble", "kib", kb::col::aliceblue);
    chan.attach_sink(console_sink);
    Channel chan_ios(Severity::Verbose, "ios", "ios", kb::col::crimson);
    chan_ios.attach_sink(console_sink);

    kfs::FileSystem filesystem(&chan_ios);
    filesystem.setup_settings_directory("ndoxx", "nuclear");
    filesystem.setup_app_data_directory("ndoxx", "nuclear");
    const auto& cfg_dir = filesystem.get_settings_directory();
    const auto& appdata_dir = filesystem.get_app_data_directory();
    klog(chan).info("Config directory:   {}", cfg_dir);
    klog(chan).info("App data directory: {}", appdata_dir);

    // Grabbing another app data directory
    // Change "vendor" and "appname" for something that exists or this will produce an error
    // klog(chan).info("Third party app data directory:\n{}", filesystem.get_app_data_directory("vendor", "appname"));

    const auto& self_dir = filesystem.get_self_directory();
    filesystem.alias_directory(self_dir / "../../data", "data");

    kfs::PackFile::pack_directory(filesystem.regular_path("data://iotest/resources"),
                                  filesystem.regular_path("data://iotest/resources.kpak"), &chan_ios);

    filesystem.alias_directory(self_dir / "../../data/iotest/resources", "resources"); // Not required
    filesystem.alias_packfile(filesystem.regular_path("data://iotest/resources.kpak"), "resources");

    {
        auto retrieved = filesystem.get_file_as_string("resources://text_file.txt");
        klog(chan).raw().debug(retrieved);
    }

    {
        auto retrieved = filesystem.get_file_as_string("resources://not_in_pack.txt");
        klog(chan).raw().debug(retrieved);
    }

    klog(chan).info("is_older(): {}", filesystem.is_older("resources://text_file.txt", "resources://not_in_pack.txt"));
    klog(chan).info("is_older(): {}", filesystem.is_older("resources://not_in_pack.txt", "resources://text_file.txt"));

    // * Syncing files
    klog(chan).info("Syncing test");
    fs::path config_source = filesystem.regular_path("data://iotest/config");
    klog(chan).info("Before sync:");
    list_dir(cfg_dir, chan);

    filesystem.sync(config_source, cfg_dir);
    klog(chan).info("After sync:");
    list_dir(cfg_dir, chan);

    // Cleanup
    fs::remove_all(cfg_dir);

    return 0;
}