#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

namespace fs = std::filesystem;
using namespace kb;

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    std::vector<math::argb32_t> heavy = {{0x13478f},{0x0d7a4f},{0x62761b},{0x141411},{0x342152},{0x831523},{0xa51e0f},{0xc4931e},{0xbbb522}};
    std::vector<math::argb32_t> ligh  = {{0x6591b8},{0x58ac91},{0x9eaa63},{0x6e6d6b},{0x9586ac},{0xb27a87},{0xc88e83},{0xc9bd7f},{0xc5c88c}};

    for(size_t ii=0; ii<heavy.size(); ++ii)
    {
        auto h_hsl = math::to_HSLA(heavy[ii]);
        auto l_hsl = math::to_HSLA(ligh[ii]);

        KLOG("nuclear",1) << KF_(heavy[ii]) << "HEAVY" << KC_ << " -> " << KF_(ligh[ii]) << "LIGHT" << std::endl;
        KLOGI << "Lfactor: " <<  l_hsl.l / h_hsl.l << " Sfactor: " << l_hsl.s / h_hsl.s << std::endl; 
    }

    auto black = math::to_CIELab(math::ColorRGBA(math::argb32_t{0x000000}));
    auto white = math::to_CIELab(math::ColorRGBA(math::argb32_t{0xffffff}));

    auto red = math::to_CIELab(math::ColorRGBA(math::argb32_t{0xff0000}));
    auto blue = math::to_CIELab(math::ColorRGBA(math::argb32_t{0x0000ff}));

    KLOGN("nuclear") << "CMETRIC" << std::endl;
    KLOG("nuclear",1) << math::delta_E_cmetric({0x000000}, {0xffffff}) << std::endl;
    KLOG("nuclear",1) << math::delta_E_cmetric({0xff0000}, {0x0000ff}) << std::endl;
    KLOGN("nuclear") << "CIE76" << std::endl;
    KLOG("nuclear",1) << math::delta_E2_CIE76(black, white) << std::endl;
    KLOG("nuclear",1) << math::delta_E2_CIE76(red, blue) << std::endl;
    KLOGN("nuclear") << "CIE94" << std::endl;
    KLOG("nuclear",1) << math::delta_E2_CIE94(black, white) << std::endl;
    KLOG("nuclear",1) << math::delta_E2_CIE94(red, blue) << std::endl;

    return 0;
}
