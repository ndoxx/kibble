#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "algorithm/endian.h"

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

    uint16_t aa = 0x1234u;
    uint64_t bb = 0x0123456789abcdefULL;
    constexpr uint16_t cc = bswap<uint16_t>(0x1234u);
    constexpr uint64_t dd = bswap(0x0123456789abcdefULL);

    KLOG("nuclear",1) << std::hex << aa << " -> " << bswap(aa) << std::endl;
    KLOG("nuclear",1) << std::hex << bb << " -> " << bswap(bb) << std::endl;
    KLOG("nuclear",1) << std::hex << cc << std::endl;
    KLOG("nuclear",1) << std::hex << dd << std::endl;

    return 0;
}
