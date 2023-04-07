#include "argparse/argparse.h"
#include "filesystem/filesystem.h"
#include "hash/hash.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"
#include "string/string.h"

#include <filesystem>
#include <fmt/std.h>
#include <iostream>
#include <map>
#include <regex>

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

void parse_entry(const fs::directory_entry&, const fs::path&, std::map<hash_t, std::string>&, kfs::FileSystem&,
                 const kb::log::Channel&);
int main(int argc, char** argv)
{
    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan_istr(Severity::Verbose, "internstring", "ist", kb::col::aliceblue);
    chan_istr.attach_sink(console_sink);

    // * Argument parsing and sanity check
    ap::ArgParse parser("internstring", "0.1");
    parser.set_log_output([&chan_istr](const std::string& str) { klog(chan_istr).uid("ArgParse").info(str); });
    const auto& a_dirpath = parser.add_positional<std::string>("DIRPATH", "Path to the root source directory");
    const auto& a_output =
        parser.add_variable<std::string>('o', "output", "Name of the output directory (default: cwd)", "");

    bool success = parser.parse(argc, argv);
    if (!success)
        show_error_and_die(parser, chan_istr);

    fs::path dirpath(a_dirpath());
    dirpath = fs::canonical(dirpath);

    if (!fs::exists(dirpath))
    {
        klog(chan_istr).fatal("Directory does not exist:\n{}", dirpath);
    }
    if (!fs::is_directory(dirpath))
    {
        klog(chan_istr).fatal("Not a directory:\n{}", dirpath);
    }

    fs::path outputdir;
    if (a_output.is_set)
        outputdir = fs::absolute(fs::path(a_output()));
    else
        outputdir = fs::current_path();

    if (!fs::exists(outputdir) || !fs::is_directory(outputdir))
    {
        klog(chan_istr).fatal("Output directory does not exist:\n{}", outputdir);
    }

    fs::path outputpath = (outputdir / "intern.txt").lexically_normal();

    // * Locate sources
    klog(chan_istr).info("Parsing sources.");
    klog(chan_istr).info("root: {}", dirpath);

    std::map<hash_t, std::string> registry;
    kfs::FileSystem filesystem;

    // Read manifest if any. If no manifest, all subdirs are explored.
    std::vector<fs::path> subdirs;
    fs::path manifest_path = dirpath / "internstring.manifest";
    if (fs::exists(manifest_path))
    {
        klog(chan_istr).info("Detected manifest.");
        auto istr = filesystem.get_input_stream(manifest_path);
        std::string line;
        while (std::getline(*istr, line))
        {
            fs::path subdirpath = dirpath / line;
            if (fs::exists(subdirpath) && fs::is_directory(subdirpath))
                subdirs.push_back(subdirpath);
        }
    }
    else
    {
        for (const auto& entry : fs::directory_iterator(dirpath))
            if (entry.is_directory())
                subdirs.push_back(entry.path());
    }

    // * Recurse
    for (auto&& subdir : subdirs)
    {
        klog(chan_istr).info("subdir  {}", subdir);
        for (const auto& entry : fs::recursive_directory_iterator(subdir))
            parse_entry(entry, subdir, registry, filesystem, chan_istr);
    }

    // * Serialize
    klog(chan_istr).info("Exporting intern string table to text file.\noutput: {}", outputpath);
    std::ofstream out_txt(outputpath);
    if (out_txt.is_open())
    {
        for (auto&& [key, value] : registry)
            out_txt << key << " " << value << std::endl;
        out_txt.close();
    }
    else
    {
        klog(chan_istr).fatal("Unable to open output text file.");
    }
    klog(chan_istr).info("Done.");

    return 0;
}

// Non greedy regex that matches the H_("any_str") macro
static const std::regex s_hash_str_tag("H_\\(\"([a-zA-Z0-9_\\.]+?)\"\\)");
// Non greedy regex that matches the "any_str"_h string literal
// BUG: seems to be greedy anyway -> will generate a faulty file
static const std::regex s_hash_str_literal_tag("\"([a-zA-Z0-9_\\.]+?)\"_h");

inline bool filter(const fs::directory_entry& entry)
{
    std::string extension = entry.path().extension().string();
    return (!extension.compare(".cpp")) || (!extension.compare(".h")) || (!extension.compare(".hpp"));
}

void register_intern_string(const std::string& intern, std::map<hash_t, std::string>& registry,
                            const kb::log::Channel& log_channel)
{
    hash_t hash_intern = H_(intern.c_str()); // Hashed string

    auto it = registry.find(hash_intern);
    if (it == registry.end())
    {
        klog(log_channel).verbose("{:<20} -> {}", hash_intern, intern);
        registry.insert(std::make_pair(hash_intern, intern));
    }
    else if (it->second.compare(intern)) // Detect hash collision
    {
        klog(log_channel)
            .warn("Hash collision detected:\n{} -> {}\n{} -> {}", it->second, it->first, intern, hash_intern);
        // Make SURE the user sees the warning
        do
        {
            std::cout << '\n' << "Press ENTER to continue...";
        } while (std::cin.get() != '\n');
    }
}

void parse_entry(const fs::directory_entry& entry, const fs::path& base, std::map<hash_t, std::string>& registry,
                 kfs::FileSystem& filesystem, const kb::log::Channel& log_channel)
{
    if (!entry.is_regular_file() || !filter(entry))
        return;

    klog(log_channel).info("reading {}", fs::relative(entry.path(), base));

    // Get file as string
    auto source = filesystem.get_file_as_string(entry.path());

    // Match hash tags in source
    {
        std::regex_iterator<std::string::iterator> it(source.begin(), source.end(), s_hash_str_tag);
        std::regex_iterator<std::string::iterator> end;

        while (it != end)
        {
            register_intern_string((*it)[1], registry, log_channel);
            ++it;
        }
    }
    {
        std::regex_iterator<std::string::iterator> it(source.begin(), source.end(), s_hash_str_literal_tag);
        std::regex_iterator<std::string::iterator> end;

        while (it != end)
        {
            register_intern_string((*it)[1], registry, log_channel);
            ++it;
        }
    }
}