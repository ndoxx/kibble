#include "logger2/channel.h"
#include "logger2/entry.h"
#include "logger2/formatters/console_formatter.h"
#include "logger2/severity.h"
#include "logger2/sinks/console_sink.h"

#include "math/color.h"
#include "math/color_table.h"
#include <fmt/color.h>
#include <fmt/core.h>

using namespace kb::log;

#define KVERB(CHAN, TEXT)                                                                                              \
    CHAN.submit(LogEntry{.severity = Severity::Verbose,                                                                \
                         .source_file = __FILE__,                                                                      \
                         .source_function = __PRETTY_FUNCTION__,                                                       \
                         .source_line = __LINE__,                                                                      \
                         .timestamp = kb::TimeBase::timestamp(),                                                       \
                         .message = TEXT});

#define KWARN(CHAN, TEXT)                                                                                              \
    CHAN.submit(LogEntry{.severity = Severity::Warn,                                                                   \
                         .source_file = __FILE__,                                                                      \
                         .source_function = __PRETTY_FUNCTION__,                                                       \
                         .source_line = __LINE__,                                                                      \
                         .timestamp = kb::TimeBase::timestamp(),                                                       \
                         .message = TEXT});

inline auto to_rgb(kb::math::argb32_t color)
{
    return fmt::rgb{uint8_t(color.r()), uint8_t(color.g()), uint8_t(color.b())};
}

std::string create_channel_tag(const std::string &short_name, kb::math::argb32_t color)
{
    return fmt::format("{}", fmt::styled(short_name, fmt::bg(to_rgb(color)) | fmt::fg(fmt::color::white)));
}

int main()
{
    auto L0 = std::make_shared<DefaultSeverityLevelPolicy>(Severity::Verbose);
    auto L3 = std::make_shared<DefaultSeverityLevelPolicy>(Severity::Warn);

    auto console_formatter = std::make_shared<ConsoleFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);

    Channel chan_render({"graphics", create_channel_tag("gfx", kb::col::crimson)});
    chan_render.attach_policy(L0);
    chan_render.attach_sink(console_sink);

    Channel chan_sound({"sound", create_channel_tag("snd", kb::col::lightorange)});
    chan_sound.attach_policy(L3);
    chan_sound.attach_sink(console_sink);

    KVERB(chan_render, "Hello");
    KWARN(chan_sound, "World");

    return 0;
}