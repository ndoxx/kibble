#include "filesystem/filesystem.h"
#include "filesystem/resource_pack.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

namespace fs = std::filesystem;

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("ios", 3));
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    init_logger();

    kfs::FileSystem filesystem;
    filesystem.setup_settings_directory("ndoxx", "nuclear");
    const auto& cfg_dir = filesystem.get_settings_directory();
    KLOG("nuclear", 1) << cfg_dir << std::endl;

    const auto& self_dir = filesystem.get_self_directory();
    filesystem.alias_directory(self_dir / "../../data", "data");

    kfs::PackFile::pack_directory(filesystem.regular_path("data://iotest/resources"),
                                  filesystem.regular_path("data://iotest/resources.kpak"));

    filesystem.alias_directory(self_dir / "../../data/iotest/resources", "resources"); // Not required
    filesystem.alias_packfile(filesystem.regular_path("data://iotest/resources.kpak"), "resources");

    {
        auto retrieved = filesystem.get_file_as_string("resources://text_file.txt");
        KLOGR("nuclear") << retrieved << std::endl;
    }

    {
        auto retrieved = filesystem.get_file_as_string("resources://not_in_pack.txt");
        KLOGR("nuclear") << retrieved << std::endl;
    }

    KLOG("nuclear", 1) << filesystem.is_older("resources://text_file.txt", "resources://not_in_pack.txt") << std::endl;
    KLOG("nuclear", 1) << filesystem.is_older("resources://not_in_pack.txt", "resources://text_file.txt") << std::endl;

    return 0;
}