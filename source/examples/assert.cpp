#define K_DEBUG
#include "assert/assert.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "logger2/sinks/file_sink.h"
#include "math/color_table.h"

using namespace kb::log;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    auto file_sink = std::make_shared<FileSink>("assert.log");
    console_sink->set_formatter(console_formatter);
    Channel chan(Severity::Verbose, "kibble", "kib", kb::col::aliceblue);
    chan.attach_sink(console_sink);
    chan.attach_sink(file_sink);

    int x = 0, y = 1;

    // * Assertions are disabled in release builds
    // Comment the #define K_DEBUG line and compile in release to make this assertion message disappear
    // * Fatal log messages will flush all sinks before terminating program
    // The file assert.log should contain the assertion message
    K_ASSERT(x > y, "x is not greater than y", &chan).watch(x).watch(y);
    klog(chan).info("If assertions are enabled, program terminates before this line");
}