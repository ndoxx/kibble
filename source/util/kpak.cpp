#include "filesystem/resource_pack.h"

#include "argparse/argparse.h"
#include "filesystem/serialization/stream_serializer.h"
#include "filesystem/stream/memory_stream.h"
#include "logger/formatters/vscode_terminal_formatter.h"
#include "logger/logger.h"
#include "logger/sinks/console_sink.h"
#include "math/color_table.h"

#include "fmt/ranges.h"
#include "fmt/std.h"
#include <filesystem>
#include <ranges>

namespace fs = std::filesystem;

using namespace kb;
using namespace kb::log;

void show_error_and_die(ap::ArgParse& parser, const Channel& chan)
{
    for (const auto& msg : parser.get_errors())
    {
        klog(chan).warn(msg);
    }

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
        parser.add_variable<std::string>('o', "output", "Name of the pack (default: ${dirname}.[kpak|h])", "");
    const auto& a_header = parser.add_flag('H', "header", "Export to C++ header file");

    bool success = parser.parse(argc, argv);
    if (!success)
    {
        show_error_and_die(parser, chan_kpak);
    }

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
    {
        output = fs::absolute(fs::path(a_output()));
    }
    else
    {
        output = dirpath.parent_path() / fmt::format("{}.{}", dirpath.stem().string(), (a_header() ? ".h" : ".kpak"));
    }

    fs::path output_parent = fs::absolute(output).parent_path();

    if (!fs::exists(output_parent))
    {
        klog(chan_kpak).fatal("Output directory does not exist:\n{}", output_parent);
    }

    klog(chan_kpak).info("Exporting pack to: {}", output);
    kb::kfs::PackFileBuilder builder;
    builder.set_logger(&chan_kpak);
    builder.add_directory(dirpath);
    size_t export_size = builder.export_size_bytes();
    std::ofstream ofs(output);

    if (!a_header())
    {
        if (builder.export_pack(ofs))
        {
            klog(chan_kpak).info("Success.");
        }
    }
    else
    {
        uint8_t* buf = new uint8_t[export_size];
        kb::OutputMemoryStream oms(buf, export_size);

        if (builder.export_pack(oms))
        {
            fmt::print(ofs,
                       "#pragma once\n\n"
                       "#include <cstddef>\n"
                       "#include <cstdint>\n\n"
                       "namespace kpak::{} {{\n\n"
                       "constexpr uint8_t kpacked_resources[] = {{\n"
                       "{}\n"
                       "}};\n\n"
                       "constexpr std::size_t kpacked_resources_size = sizeof(kpacked_resources);\n\n"
                       "}}\n",
                       dirpath.stem().string(), [buf, export_size]() {
                           std::string result;
                           for (size_t ii = 0; ii < export_size; ++ii)
                           {
                               if (ii % 12 == 0 && ii != 0)
                               {
                                   result += "\n    ";
                               }
                               result += fmt::format("0x{:02x}", buf[ii]);
                               if (ii != export_size - 1)
                               {
                                   result += ", ";
                               }
                           }
                           return result;
                       }());

            /* C++23: replace lambda by
                fmt::join(buffer | std::views::transform([](uint8_t b) {
                    return fmt::format("0x{:02x}", b);
                }) | std::views::chunk(12), ",\n    ")
            */
            klog(chan_kpak).info("Success.");
        }

        delete[] buf;
    }

    return 0;
}