#include "argparse/argparse.h"
#include "filesystem/filesystem.h"
#include "filesystem/resource_pack.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "string/string.h"

#include <filesystem>
namespace fs = std::filesystem;

using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("kpak", 3));
    KLOGGER(create_channel("ios", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

void show_error_and_die(ap::ArgParse& parser)
{
    for(const auto& msg : parser.get_errors())
        KLOGW("kpak") << msg << std::endl;

    KLOG("kpak", 1) << parser.usage() << std::endl;
    exit(0);
}

int main(int argc, char** argv)
{
    init_logger();
    ap::ArgParse parser("kpak", "0.1");
    parser.set_log_output([](const std::string& str) { KLOG("kpak", 1) << str << std::endl; });
    const auto& a_dirpath = parser.add_positional<std::string>("DIRPATH", "Path to the directory to pack");
    const auto& a_output = parser.add_variable<std::string>('o', "output", "Name of the pack (default: <dirname>.kpak)", "");

    bool success = parser.parse(argc, argv);
    if(!success) show_error_and_die(parser);

    fs::path dirpath(a_dirpath());
    dirpath = fs::canonical(dirpath);

    if(!fs::exists(dirpath))
    {
        KLOGE("kpak") << "Directory does not exist:" << std::endl;
        KLOGI << KS_PATH_ << dirpath << std::endl;
        exit(0);
    }
    if(!fs::is_directory(dirpath))
    {
        KLOGE("kpak") << "Not a directory:" << std::endl;
        KLOGI << KS_PATH_ << dirpath << std::endl;
        exit(0);
    }

    fs::path output;
    if(a_output.is_set)
        output = fs::absolute(fs::path(a_output()));
    else
        output = dirpath.parent_path() / su::concat(dirpath.stem().string(), ".kpak");
    

    fs::path output_parent = fs::absolute(output).parent_path();

    if(!fs::exists(output_parent))
    {
        KLOGE("kpak") << "Output directory does exist:" << std::endl;
        KLOGI << KS_PATH_ << output_parent << std::endl;
        exit(0);
    }

    kfs::PackFile::pack_directory(dirpath, output);

    return 0;
}