#include "argparse/argparse.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "string/string.h"
#include "filesystem/filesystem.h"

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

    KLOG("nuclear",1) << "Self directory:" << std::endl;
    KLOGI << WCC('p') << self_dir << std::endl;

    filesystem.add_directory_alias(self_dir / "../../data", "data");
    KLOG("nuclear",1) << "Retrieving aliased directory:" << std::endl;
    KLOGI << WCC('p') << filesystem.get_aliased_directory("data"_h) << std::endl;

    auto client_cfg_filepath = filesystem.universal_path("data://config/client.toml");
    KLOG("nuclear",1) << "Retrieving file path using a universal path string:" << std::endl;
    KLOGI << WCC('p') << client_cfg_filepath << std::endl;


    return 0;
}