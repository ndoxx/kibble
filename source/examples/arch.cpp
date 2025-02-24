#include "kibble/logger/formatters/powerline_terminal_formatter.h"
#include "kibble/logger/logger.h"
#include "kibble/logger/sinks/console_sink.h"
#include "kibble/math/color_table.h"

#include "kibble/platform/arch.h"

using namespace kb;
using namespace kb::log;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(std::make_shared<PowerlineTerminalFormatter>());
    Channel chan(Severity::Verbose, "kibble", "kbl", kb::col::aliceblue);
    chan.attach_sink(console_sink);

    CPUInfo::dump(chan);

    return 0;
}
