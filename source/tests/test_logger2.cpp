#include "common/utils.hpp"

#define K_DEBUG // For verbose / debug to actually do something
#include "logger2/logger.h"
#include "logger2/sink.h"
#include "math/color_table.h"
#include <catch2/catch_all.hpp>

using namespace kb::log;

class MockSink : public Sink
{
public:
    void submit(const LogEntry& e, const ChannelPresentation& p) override
    {
        e_ = e;
        p_ = p;
    }

    LogEntry e_;
    ChannelPresentation p_;
};

class SinkFixture
{
public:
    SinkFixture() : chan(Severity::Verbose, "test", "tst", kb::col::aliceblue), sink(new MockSink)
    {
        chan.attach_sink(sink);
        Channel::exit_on_fatal_error(false);
    }

protected:
    Channel chan;
    std::shared_ptr<MockSink> sink;
};

TEST_CASE_METHOD(SinkFixture, "Properties test", "[sink]")
{
    // clang-format off
    klog(chan).verbose("Message"); int line = __LINE__;
    // clang-format on
    REQUIRE(sink->e_.message == "Message");
    REQUIRE(sink->e_.source_location.line == line);
    REQUIRE(sink->e_.thread_id == 0xffffffff);
    REQUIRE(sink->e_.uid_text.empty());
}

TEST_CASE_METHOD(SinkFixture, "Verbose test", "[sink]")
{
    klog(chan).verbose("Message");
    REQUIRE(sink->e_.severity == Severity::Verbose);
}

TEST_CASE_METHOD(SinkFixture, "Debug test", "[sink]")
{
    klog(chan).debug("Message");
    REQUIRE(sink->e_.severity == Severity::Debug);
}

TEST_CASE_METHOD(SinkFixture, "Info test", "[sink]")
{
    klog(chan).info("Message");
    REQUIRE(sink->e_.severity == Severity::Info);
}

TEST_CASE_METHOD(SinkFixture, "Warning test", "[sink]")
{
    klog(chan).warn("Message");
    REQUIRE(sink->e_.severity == Severity::Warn);
}

TEST_CASE_METHOD(SinkFixture, "Error test", "[sink]")
{
    klog(chan).error("Message");
    REQUIRE(sink->e_.severity == Severity::Error);
}

TEST_CASE_METHOD(SinkFixture, "Fatal test", "[sink]")
{
    klog(chan).fatal("Message");
    REQUIRE(sink->e_.severity == Severity::Fatal);
}