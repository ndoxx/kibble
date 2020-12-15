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
    filesystem.add_directory_alias(self_dir / "../../data", "data");

    kfs::pack_directory(filesystem.universal_path("data://iotest/resources"),
                        filesystem.universal_path("data://iotest/resources.kpak"));

    kfs::PackFile pack(filesystem.universal_path("data://iotest/resources.kpak"));

    const auto& text_file_entry = pack.get_entry("text_file.txt");
    print_entry(text_file_entry);

    {
        std::ifstream ifs(filesystem.universal_path("data://iotest/resources/text_file.txt"), std::ios::binary);
        auto tmp = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        KLOGR("nuclear") << tmp << std::endl;
    }

    {
        auto pstream = pack.get_input_stream("text_file.txt");
        auto tmp = std::string((std::istreambuf_iterator<char>(*pstream)), std::istreambuf_iterator<char>());
        KLOGR("nuclear") << tmp << std::endl;
    }

    {
        auto pstream = pack.get_input_stream("textures/default.dat");
        auto tmp = std::vector<char>((std::istreambuf_iterator<char>(*pstream)), std::istreambuf_iterator<char>());
        KLOG("nuclear", 1) << WCC('v');
        for(size_t ii = 0; ii < tmp.size(); ++ii)
        {
            KLOG("nuclear", 1) << std::setw(4) << int(static_cast<unsigned char>(tmp[ii])) << ' ';
        }
        KLOG("nuclear", 1) << std::endl;
    }

    return 0;
}