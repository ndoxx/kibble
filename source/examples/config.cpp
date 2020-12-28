#include "config/config.h"
#include "argparse/argparse.h"
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
    KLOGGER(create_channel("config", 3));
    KLOGGER(create_channel("kibble", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

void show_error_and_die(ap::ArgParse& parser)
{
    for(const auto& msg : parser.get_errors())
        KLOGW("kibble") << msg << std::endl;

    KLOG("kibble", 1) << parser.usage() << std::endl;
    exit(0);
}

int main(int argc, char** argv)
{
    init_logger();

    ap::ArgParse parser("nuclear", "0.1");
    const auto& cfg_path_str = parser.add_positional<std::string>("CONFIG_PATH", "Path to the config directory");
    bool success = parser.parse(argc, argv);
    if(!success)
        show_error_and_die(parser);

    fs::path cfg_path(cfg_path_str());
    if(!fs::exists(cfg_path))
    {
        KLOGE("nuclear") << "Directory does not exist:" << std::endl;
        KLOGI << KS_PATH_ << cfg_path << std::endl;
        return 0;
    }
    if(!fs::is_directory(cfg_path))
    {
        KLOGE("nuclear") << "Not a directory:" << std::endl;
        KLOGI << KS_PATH_ << cfg_path << std::endl;
        return 0;
    }

    cfg::Settings settings;
    for(auto& file : fs::directory_iterator(cfg_path))
    {
        if(fs::is_regular_file(file) && !file.path().extension().string().compare(".toml"))
            settings.load_toml(file);
    }

#ifdef K_DEBUG
    KLOGN("nuclear") << "Full dump:" << std::endl;
    settings.debug_dump();
#endif

    KLOGN("nuclear") << "Displaying some properties:" << std::endl;

    KLOG("nuclear", 1) << "client window title:  " << settings.get<std::string>("client.window.title"_h, "MaBalls")
                       << std::endl;
    KLOG("nuclear", 1) << "client window width:  " << settings.get<int64_t>("client.window.width"_h, 1024) << std::endl;
    KLOG("nuclear", 1) << "client window width:  " << settings.get<size_t>("client.window.width"_h, 1024) << std::endl;
    KLOG("nuclear", 1) << "client window height: " << settings.get<size_t>("client.window.height"_h, 768) << std::endl;
    KLOG("nuclear", 1) << "unknown property:     " << settings.get<size_t>("client.window.i_dont_exist"_h, 42)
                       << std::endl;

    KLOGN("nuclear") << "Displaying array properties:" << std::endl;
    for(size_t ii = 0; ii < settings.get_array_size("erwin.logger.channels"_h); ++ii)
    {
        std::string channel_name = settings.get<std::string>(su::h_concat("erwin.logger.channels[", ii, "].name"), "");
        size_t verbosity = settings.get<size_t>(su::h_concat("erwin.logger.channels[", ii, "].verbosity"), 0);

        KLOG("nuclear", 1) << "Channel #" << channel_name << ": verb= " << verbosity << std::endl;
    }

    KLOGN("nuclear") << "Modifying and serializing data" << std::endl;
    settings.set<size_t>("mutable.player.hp"_h, 88888);
    settings.set<std::string>("mutable.player.location"_h, "behind you");

    settings.save_toml(cfg_path / "mutable.toml");

    return 0;
}