#include "argparse/argparse.h"
#include "filesystem/filesystem.h"
#include "filesystem/resource_pack.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"
#include "string/string.h"

#include <filesystem>
#include <fmt/std.h>

namespace fs = std::filesystem;

using namespace kb;
using namespace kb::log;

void show_error_and_die(ap::ArgParse& parser, const Channel& chan)
{
    for (const auto& msg : parser.get_errors())
        klog(chan).warn(msg);

    klog(chan).raw().info(parser.usage());
    exit(0);
}

int main(int argc, char** argv)
{
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan_kpak(Severity::Verbose, "kpak", "kpk", kb::col::aliceblue);
    chan_kpak.attach_sink(console_sink);
    Channel chan_ios(Severity::Verbose, "ios", "ios", kb::col::crimson);
    chan_ios.attach_sink(console_sink);

    ap::ArgParse parser("kpak", "0.1");
    parser.set_log_output([&chan_kpak](const std::string& str) { klog(chan_kpak).uid("ArgParse").info(str); });
    const auto& a_dirpath = parser.add_positional<std::string>("DIRPATH", "Path to the directory to pack");
    const auto& a_output =
        parser.add_variable<std::string>('o', "output", "Name of the pack (default: <dirname>.kpak)", "");

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan_kpak);

    fs::path dirpath(a_dirpath());
    dirpath = fs::canonical(dirpath);

    if (!fs::exists(dirpath))
    {
        klog(chan_kpak).fatal("Directory does not exist:\n{}", dirpath);
    }
    if (!fs::is_directory(dirpath))
    {
        klog(chan_kpak).fatal("Not a directory:\n{}", dirpath);
    }

    fs::path output;
    if (a_output.is_set)
        output = fs::absolute(fs::path(a_output()));
    else
        output = dirpath.parent_path() / su::concat(dirpath.stem().string(), ".kpak");

    fs::path output_parent = fs::absolute(output).parent_path();

    if (!fs::exists(output_parent))
    {
        klog(chan_kpak).fatal("Output directory does not exist:\n{}", output_parent);
    }

    kfs::PackFile::pack_directory(dirpath, output, &chan_kpak);

    return 0;
}