#include "argparse/argparse.h"
#include "filesystem/filesystem.h"
#include "filesystem/resource_pack.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "string/string.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <numeric>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

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

void print_entry(const kfs::PackLocalEntry& entry)
{
    KLOG("nuclear", 1) << WCC('p') << entry.path << std::endl;
    KLOGI << "Offset: " << entry.offset << std::endl;
    KLOGI << "Size:   " << entry.size << std::endl;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    init_logger();

    kfs::FileSystem filesystem;
    const auto& self_dir = filesystem.get_self_directory();
    filesystem.alias_directory(self_dir / "../../data", "data");

    /*kfs::pack_directory(filesystem.regular_path("data://iotest/resources"),
                        filesystem.regular_path("data://iotest/resources.kpak"));*/

    filesystem.alias_directory(self_dir / "../../data/iotest/resources", "resources"); // Not required
    filesystem.alias_packfile(filesystem.regular_path("data://iotest/resources.kpak"), "resources");

    {
        auto pstream = filesystem.get_input_stream("resources://text_file.txt");
        auto retrieved = std::string((std::istreambuf_iterator<char>(*pstream)), std::istreambuf_iterator<char>());
        KLOG("nuclear", 1) << retrieved << std::endl;
    }

    {
        auto pstream = filesystem.get_input_stream("resources://another_file.txt");
        auto retrieved = std::string((std::istreambuf_iterator<char>(*pstream)), std::istreambuf_iterator<char>());
        KLOG("nuclear", 1) << retrieved << std::endl;
    }

    return 0;
}