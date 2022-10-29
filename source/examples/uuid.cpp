#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "random/uuid.h"
#include "random/xor_shift.h"

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

    UUIDv4::UUIDGenerator<rng::XorShiftEngine> gen;

    for(size_t ii=0; ii<10; ++ii)
    {
        KLOG("nuclear",1) << gen() << std::endl;
    }

    return 0;
}
