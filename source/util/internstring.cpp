#include "argparse/argparse.h"
#include "filesystem/filesystem.h"
#include "hash/hash.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "string/string.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <regex>
namespace fs = std::filesystem;

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("ist", 3));
    KLOGGER(create_channel("ios", 0));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

void show_error_and_die(ap::ArgParse& parser)
{
    for(const auto& msg : parser.get_errors())
        KLOGW("ist") << msg << std::endl;

    KLOG("ist", 1) << parser.usage() << std::endl;
    exit(0);
}

void parse_entry(const fs::directory_entry&, const fs::path&, std::map<hash_t, std::string>&, kfs::FileSystem&);
int main(int argc, char** argv)
{
    init_logger();

    // * Argument parsing and sanity check
    ap::ArgParse parser("internstring", "0.1");
    parser.set_log_output([](const std::string& str) { KLOG("ist", 1) << str << std::endl; });
    const auto& a_dirpath = parser.add_positional<std::string>("DIRPATH", "Path to the root source directory");
    const auto& a_output =
        parser.add_variable<std::string>('o', "output", "Name of the output directory (default: cwd)", "");

    bool success = parser.parse(argc, argv);
    if(!success)
        show_error_and_die(parser);

    fs::path dirpath(a_dirpath());
    dirpath = fs::canonical(dirpath);

    if(!fs::exists(dirpath))
    {
        KLOGE("ist") << "Directory does not exist:" << std::endl;
        KLOGI << KS_PATH_ << dirpath << std::endl;
        exit(0);
    }
    if(!fs::is_directory(dirpath))
    {
        KLOGE("ist") << "Not a directory:" << std::endl;
        KLOGI << KS_PATH_ << dirpath << std::endl;
        exit(0);
    }

    fs::path outputdir;
    if(a_output.is_set)
        outputdir = fs::absolute(fs::path(a_output()));
    else
        outputdir = fs::current_path();

    if(!fs::exists(outputdir) || !fs::is_directory(outputdir))
    {
        KLOGE("ist") << "Output directory does exist:" << std::endl;
        KLOGI << KS_PATH_ << outputdir << std::endl;
        exit(0);
    }

    fs::path outputpath = (outputdir / "intern.txt").lexically_normal();

    // * Locate sources
    KLOGN("ist") << "Parsing sources." << std::endl;
    KLOGI << "root: " << KS_PATH_ << dirpath << std::endl;

    std::map<hash_t, std::string> registry;
    kfs::FileSystem filesystem;

    // Read manifest if any. If no manifest, all subdirs are explored.
    std::vector<fs::path> subdirs;
    fs::path manifest_path = dirpath / "internstring.manifest";
    if(fs::exists(manifest_path))
    {
        KLOGN("ist") << "Detected manifest." << std::endl;
        auto istr = filesystem.get_input_stream(manifest_path);
        std::string line;
        while(std::getline(*istr, line))
        {
            fs::path subdirpath = dirpath / line;
            if(fs::exists(subdirpath) && fs::is_directory(subdirpath))
                subdirs.push_back(subdirpath);
        }
    }
    else
    {
        for(const auto& entry : fs::directory_iterator(dirpath))
            if(entry.is_directory())
                subdirs.push_back(entry.path());
    }

    // * Recurse
    for(auto&& subdir : subdirs)
    {
        KLOG("ist", 1) << KS_INST_ << "subdir  " << KS_PATH_ << subdir << std::endl;
        for(const auto& entry : fs::recursive_directory_iterator(subdir))
            parse_entry(entry, subdir, registry, filesystem);
    }

    // * Serialize
    KLOGN("ist") << "Exporting intern string table to text file." << std::endl;
    KLOGI << "output: " << KS_PATH_ << outputpath << std::endl;
    std::ofstream out_txt(outputpath);
    if(out_txt.is_open())
    {
        for(auto&& [key, value] : registry)
            out_txt << key << " " << value << std::endl;
        out_txt.close();
    }
    else
    {
        KLOGE("ist") << "Unable to open output text file." << std::endl;
        exit(0);
    }
    KLOGG("ist") << "Done." << std::endl;

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

void register_intern_string(const std::string& intern, std::map<hash_t, std::string>& registry)
{
    hash_t hash_intern = H_(intern.c_str()); // Hashed string

    auto it = registry.find(hash_intern);
    if(it == registry.end())
    {
        KLOGI << std::left << std::setw(20) << hash_intern << " -> " << KF_({0x669900}) << intern
              << std::endl;
        registry.insert(std::make_pair(hash_intern, intern));
    }
    else if(it->second.compare(intern)) // Detect hash collision
    {
        KLOGW("ist") << "Hash collision detected:" << std::endl;
        KLOGI << it->second << " -> " << it->first << std::endl;
        KLOGI << intern << " -> " << hash_intern << std::endl;
        // Make SURE the user sees the warning
        do
        {
            std::cout << '\n' << "Press ENTER to continue...";
        } while(std::cin.get() != '\n');
    }
}

void parse_entry(const fs::directory_entry& entry, const fs::path& base, std::map<hash_t, std::string>& registry,
                 kfs::FileSystem& filesystem)
{
    if(!entry.is_regular_file() || !filter(entry))
        return;

    KLOG("ist", 1) << KS_INST_ << "reading " << KS_PATH_ << fs::relative(entry.path(), base) << std::endl;

    // Get file as string
    auto source = filesystem.get_file_as_string(entry.path());

    // Match hash tags in source
    {
        std::regex_iterator<std::string::iterator> it(source.begin(), source.end(), s_hash_str_tag);
        std::regex_iterator<std::string::iterator> end;

        while(it != end)
        {
            register_intern_string((*it)[1], registry);
            ++it;
        }
    }
    {
        std::regex_iterator<std::string::iterator> it(source.begin(), source.end(), s_hash_str_literal_tag);
        std::regex_iterator<std::string::iterator> end;

        while(it != end)
        {
            register_intern_string((*it)[1], registry);
            ++it;
        }
    }
}