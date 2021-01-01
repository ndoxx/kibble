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

    for(size_t ii=0; ii<10; ++ii)
    {
        float tt = float(ii)/float(10-1);
        math::ColorHSLA hsla(tt,1.f,0.5f);

        auto rgba1 = math::to_RGBA(hsla);
        auto hsla2 = math::to_HSLA(rgba1);
        auto rgba2 = math::to_RGBA(hsla2);

        KLOG("nuclear",1) << KF_(math::pack_ARGB(rgba1)) << "RGBA1 " << KF_(math::pack_ARGB(rgba2)) << "RGBA2" << std::endl;
    }

    return 0;
}
