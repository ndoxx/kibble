#include "argparse/argparse.h"
#include "filesystem/filesystem.h"
#include "filesystem/resource_pack.h"
#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "string/string.h"
#include "math/color.h"
#include "math/constexpr_math.h"
#include "hash/hash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
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

    using namespace kb::math;
    for(size_t ii=0; ii<10; ++ii)
    {
        KLOG("nuclear",1) << KF_(pack_ARGB(ColorHSLA::random_hue(0.8f,0.5f))) << "Plop" << std::endl;
    }

    KLOG("nuclear",1) << KF_(col::pink) << "Pink" << std::endl;

    constexpr math::argb32_t mycol = pack_ARGB(128,128,128);
    constexpr math::argb32_t mycol2 = pack_ARGB(math::ColorRGBA(0.5f,0.5f,0.5f));

    KLOG("nuclear",1) << KF_(mycol) << "Plop" << std::endl;
    KLOG("nuclear",1) << KF_(mycol2) << "Plop" << std::endl;


    return 0;
}