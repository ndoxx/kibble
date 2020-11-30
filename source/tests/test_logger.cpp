#include <thread>
#include <iostream>
#include <string_view>

#include "logger/logger.h"
#include "logger/sink.h"
#include "logger/dispatcher.h"

#include "time/clock.h"

using namespace kb;

static const std::array<std::string,15> k_channels =
{
	"application",
	"editor",
	"event",
	"asset",
	"memory",
	"thread",
	"entity",
	"scene",
	"script",
	"render",
	"shader",
	"texture",
	"util",
	"config",
	"ios"
};

void init_logger()
{
	KLOGGER_START();
	
	for(size_t ii=0; ii<k_channels.size(); ++ii)
	{
    	KLOGGER(create_channel(k_channels[ii], 3));
	}

    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main()
{
	init_logger();

    KLOGN("core") << "-------- [CHANNELS] --------" << std::endl;
	KLOG("core",1) << "Hello" << std::endl;
	KLOG("application",1) << "Hello" << std::endl;
	KLOG("editor",1) << "Hello" << std::endl;
	KLOG("event",1) << "Hello" << std::endl;
	KLOG("asset",1) << "Hello" << std::endl;
	KLOG("memory",1) << "Hello" << std::endl;
	KLOG("thread",1) << "Hello" << std::endl;
	KLOG("entity",1) << "Hello" << std::endl;
	KLOG("scene",1) << "Hello" << std::endl;
	KLOG("script",1) << "Hello" << std::endl;
	KLOG("render",1) << "Hello" << std::endl;
	KLOG("shader",1) << "Hello" << std::endl;
	KLOG("texture",1) << "Hello" << std::endl;
	KLOG("util",1) << "Hello" << std::endl;
	KLOG("config",1) << "Hello" << std::endl;
	KLOG("ios",1) << "Hello" << std::endl;


    KLOGN("core") << "-------- [COLORS] --------" << std::endl;
    KLOG("core",1) << "Configuring " << WCC('i') << "accessibility" << WCC(0) << " parameters." << std::endl;
    KLOG("core",1) << "If you are " << WCC('x') << "colorblind" << WCC(0)
                            << " you can't see " << WCC('g') << "this" << WCC(0) << ":" << std::endl;

    for(uint8_t ii=0; ii<10; ++ii)
    {
        for(uint8_t jj=0; jj<10; ++jj)
            KLOG("core",1) << WCC(25*ii,25*jj,255-25*jj) << char('A'+ii+jj) << " ";
        KLOG("core",1) << std::endl;
    }


    KLOGN("core") << "-------- [SEVERITY & ERROR REPORT] --------" << std::endl;
    KBANG();
    KLOGN("render") << "Notification message" << std::endl;
    KLOGI           << "Item 1" << std::endl;
    KLOGI           << "Item 2" << std::endl;
    KLOGI           << "Item 3" << std::endl;
    KLOGW("core") << "Warning message" << std::endl;
    KLOGE("core") << "Error message" << std::endl;
    KLOGF("core") << "Fatal error message" << std::endl;

/*
    milliClock clk;
    clk.restart();
    std::this_thread::sleep_for(std::chrono::milliseconds(512));
    auto dur = clk.get_elapsed_time();

    KLOG("core",1) << dur.count() << "ms" << std::endl;
*/

	return 0;
}