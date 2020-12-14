#include "argparse/argparse.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "string/string.h"
#include "filesystem/filesystem.h"
#include "filesystem/resource_pack.h"

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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    init_logger();

    kfs::FileSystem filesystem;
    const auto& self_dir = filesystem.get_self_directory();
    filesystem.add_directory_alias(self_dir / "../../data", "data");

    kfs::pack_directory(filesystem.universal_path("data://iotest/resources"), 
                        filesystem.universal_path("data://iotest/resources.kpak"));

    return 0;
}