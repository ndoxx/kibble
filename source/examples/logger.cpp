#include <iostream>
#include <thread>

#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#include "time/clock.h"

using namespace kb;

const std::array<std::string, 15> k_channels = {"application", "editor",  "event", "asset",  "memory",
                                                "thread",      "entity",  "scene", "script", "render",
                                                "shader",      "texture", "util",  "config", "ios"};

void init_logger()
{
    KLOGGER_START();

    for(size_t ii = 0; ii < k_channels.size(); ++ii)
    {
        KLOGGER(create_channel(k_channels[ii], 3));
    }

    KLOGGER(create_channel("custom", 3));
    KLOGGER(set_channel_tag("custom", "csm", kb::col::darkorchid));

    KLOGGER(attach_all("console_sink", std::make_unique<log_deprec::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main()
{
    init_logger();

    KLOGR("core") << "Raw output:" << std::endl;
    KLOGR("core") << "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
                     "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
                     "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in "
                     "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat "
                     "non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
                  << std::endl;

    KLOGN("core") << "-------- [CHANNELS] --------" << std::endl;
    KLOG("core", 1) << "Hello from 'core'" << std::endl;
    KLOG("application", 1) << "Hello from 'application'" << std::endl;
    KLOG("editor", 1) << "Hello from 'editor'" << std::endl;
    KLOG("event", 1) << "Hello from 'event'" << std::endl;
    KLOG("asset", 1) << "Hello from 'asset'" << std::endl;
    KLOG("memory", 1) << "Hello from 'memory'" << std::endl;
    KLOG("thread", 1) << "Hello from 'thread'" << std::endl;
    KLOG("entity", 1) << "Hello from 'entity'" << std::endl;
    KLOG("scene", 1) << "Hello from 'scene'" << std::endl;
    KLOG("script", 1) << "Hello from 'script'" << std::endl;
    KLOG("render", 1) << "Hello from 'render'" << std::endl;
    KLOG("shader", 1) << "Hello from 'shader'" << std::endl;
    KLOG("texture", 1) << "Hello from 'texture'" << std::endl;
    KLOG("util", 1) << "Hello from 'util'" << std::endl;
    KLOG("config", 1) << "Hello from 'config'" << std::endl;
    KLOG("ios", 1) << "Hello from 'ios'" << std::endl;
    KLOG("custom", 1) << "Hello from custom style channel" << std::endl;

    KLOGN("core") << "-------- [COLORS] --------" << std::endl;
    KLOG("core", 1) << "Configuring " << KS_INST_ << "accessibility" << KC_ << " parameters." << std::endl;
    KLOG("core", 1) << "If you are " << KS_NODE_ << "colorblind" << KC_ << " you can't see " << KF_(col::lawngreen)
                    << "this" << KC_ << ":" << std::endl;

    for(uint8_t ii = 0; ii < 10; ++ii)
    {
        for(uint8_t jj = 0; jj < 10; ++jj)
            KLOG("core", 1) << KF_(25 * ii, 25 * jj, 255 - 25 * jj) << char('A' + ii + jj) << " ";
        KLOG("core", 1) << std::endl;
    }

    KLOGN("core") << "-------- [SEMIOTICS] --------" << std::endl;
    KLOG("core", 1) << KS_PATH_ << "\"path/to/some/file\"" << std::endl;
    KLOG("core", 1) << KS_INST_ << "action" << std::endl;
    KLOG("core", 1) << KS_DEFL_ << "default" << std::endl;
    KLOG("core", 1) << KS_NAME_ << "name" << std::endl;
    KLOG("core", 1) << "a value: " << KS_VALU_ << 123 << std::endl;
    KLOG("core", 1) << "an important value: " << KS_IVAL_ << 1234 << std::endl;
    KLOG("core", 1) << KS_ATTR_ << "attribute" << std::endl;
    KLOG("core", 1) << KS_NODE_ << "node" << std::endl;
    KLOG("core", 1) << KS_HIGH_ << "emphasis" << std::endl;
    KLOG("core", 1) << KS_POS_ << "this is good" << std::endl;
    KLOG("core", 1) << KS_NEG_ << "this is bad" << std::endl;

    KLOGN("core") << "-------- [SEVERITY & ERROR REPORT] --------" << std::endl;
    KBANG();
    KLOGN("render") << "Notification message" << std::endl;
    KLOGI << "Item 1" << std::endl;
    KLOGI << "Item 2" << std::endl;
    KLOGI << "Item 3" << std::endl;
    KLOGW("core") << "Warning message" << std::endl;
    KLOGE("core") << "Error message" << std::endl;
    KLOGF("core") << "Fatal error message" << std::endl;

    return 0;
}