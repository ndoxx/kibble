#include "config/config.h"
#include "argparse/argparse.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"
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
#include <fmt/std.h>

namespace fs = std::filesystem;

using namespace kb;
using namespace kb::log;

void show_error_and_die(ap::ArgParse &parser, const Channel& chan)
{
    for (const auto &msg : parser.get_errors())
        klog2(chan).warn(msg);

    klog2(chan).raw().info(parser.usage());
    exit(0);
}

int main(int argc, char **argv)
{
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan(Severity::Verbose, "kibble", "kib", kb::col::aliceblue);
    chan.attach_sink(console_sink);
    Channel chan_settings(Severity::Verbose, "settings", "set", kb::col::crimson);
    chan_settings.attach_sink(console_sink);

    ap::ArgParse parser("nuclear", "0.1");
    const auto &cfg_path_str = parser.add_positional<std::string>("CONFIG_PATH", "Path to the config directory");
    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan);

    fs::path cfg_path(cfg_path_str());
    if (!fs::exists(cfg_path))
    {
        klog2(chan).error("Directory does not exist:\n{}", cfg_path);
        return 0;
    }
    if (!fs::is_directory(cfg_path))
    {
        klog2(chan).error("Not a directory:\n{}", cfg_path);
        return 0;
    }

    cfg::Settings settings(&chan_settings);
    for (auto &file : fs::directory_iterator(cfg_path))
    {
        if (fs::is_regular_file(file) && !file.path().extension().string().compare(".toml"))
            settings.load_toml(file);
    }

    settings.debug_dump();

    klog2(chan).info("Displaying some properties:");
    klog2(chan).verbose("client window title:  {}", settings.get<std::string>("client.window.title"_h, "MaBalls"));
    klog2(chan).verbose("client window width:  {}", settings.get<int64_t>("client.window.width"_h, 1024));
    klog2(chan).verbose("client window width:  {}", settings.get<size_t>("client.window.width"_h, 1024));
    klog2(chan).verbose("client window height: {}", settings.get<size_t>("client.window.height"_h, 768));
    klog2(chan).verbose("unknown property:     {}", settings.get<size_t>("client.window.i_dont_exist"_h, 42));

    klog2(chan).info("Displaying array properties:");
    for (size_t ii = 0; ii < settings.get_array_size("erwin.logger.channels"_h); ++ii)
    {
        std::string channel_name = settings.get<std::string>(su::h_concat("erwin.logger.channels[", ii, "].name"), "");
        size_t verbosity = settings.get<size_t>(su::h_concat("erwin.logger.channels[", ii, "].verbosity"), 0);

        klog2(chan).verbose("Channel #{}: verb= {}", channel_name, verbosity);
    }

    klog2(chan).info("Modifying and serializing data");
    settings.set<size_t>("mutable.player.hp"_h, 88888);
    settings.set<std::string>("mutable.player.location"_h, "behind you");

    settings.save_toml(cfg_path / "mutable.toml");

    return 0;
}