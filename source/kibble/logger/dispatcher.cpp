#include <array>
#include <cmath>
#include <iostream>

#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "logger/stack_trace.h"

namespace kb
{
namespace log_deprec
{

static constexpr float k_light = 0.75f;
static constexpr size_t k_palette_sz = 16;
static constexpr std::array<uint32_t, k_palette_sz> k_palette = {
    0xffb400, 0xffde00, 0xd7ea02, 0x85ea02, 0x20efa8, 0x20efed, 0x20bcef, 0x2073ef,
    0x6920ef, 0xad20ef, 0xe820ef, 0xef209f, 0xef209f, 0xef209f, 0xef2020, 0xbababa};

std::string create_channel_tag(const std::string &short_name, math::argb32_t color)
{
    std::stringstream ss;
    ss << "\033[1;48;2;" << color.r() << ";" << color.g() << ";" << color.b() << "m"
       << "[" << short_name << "]\033[0m";
    return ss.str();
}

LogDispatcher::LogDispatcher() : backtrace_on_error_(false)
{
    create_channel("core", 3);
    std::cerr << KF_(255, 175, 0) << "/!\\ This logger is deprecated. You should use logger2 instead!" << std::endl;
}

LogDispatcher::~LogDispatcher()
{
    for (auto &&[key, sink] : sinks_)
        sink->finish();
}

void LogDispatcher::has_channel(hash_t hname, bool &result)
{
    result = (channels_.find(hname) != channels_.end());
}

void LogDispatcher::create_channel(const std::string &name, uint8_t verbosity)
{
    hash_t hname = H_(name.c_str());

    // Detect duplicate channel or hash collision
    auto it = channels_.find(hname);
    if (it != channels_.end())
    {
        std::cout << "Duplicate channel (or hash collision) -> ignoring channel \'" << it->second.name << "\'"
                  << std::endl;
        return;
    }

    size_t pal_idx = channels_.size() % k_palette_sz;
    auto tag = create_channel_tag(name.substr(0, 3), math::lighten(math::argb32_t{k_palette[pal_idx]}, k_light));

    channels_.insert(std::make_pair(H_(name.c_str()), LogChannel{verbosity, name, tag}));
}

void LogDispatcher::set_channel_tag(const std::string &name, const std::string &custom_short_name, math::argb32_t color)
{
    std::unique_lock<std::mutex> lock(mutex_);

    auto findit = channels_.find(H_(name));
    if (findit != channels_.end())
        findit->second.tag = create_channel_tag(custom_short_name.substr(0, 3), color);
}

void LogDispatcher::attach(const std::string &sink_name, std::unique_ptr<Sink> sink,
                           const std::vector<hash_t> &channels)
{
    hash_t hsink = H_(sink_name.c_str());
    sinks_.insert(std::make_pair(hsink, std::move(sink)));
    auto &sink_ref = sinks_.at(hsink);

    for (hash_t channel : channels)
    {
        sink_subscriptions_.insert(std::make_pair(channel, hsink));
        sink_ref->add_channel_subscription({channel, channels_.at(channel).name});
    }
    sink_ref->on_attach();
}

void LogDispatcher::attach_all(const std::string &sink_name, std::unique_ptr<Sink> sink)
{
    hash_t hsink = H_(sink_name.c_str());
    sinks_.insert(std::make_pair(hsink, std::move(sink)));
    auto &sink_ref = sinks_.at(hsink);

    for (auto &&[key, chan] : channels_)
    {
        sink_subscriptions_.insert(std::make_pair(key, hsink));
        sink_ref->add_channel_subscription({key, chan.name});
    }
    sink_ref->on_attach();
}

void LogDispatcher::set_sink_enabled(hash_t name, bool value)
{
    std::unique_lock<std::mutex> lock(mutex_);
    sinks_.at(name)->set_enabled(value);
}

void LogDispatcher::dispatch(const LogStatement &stmt)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (stmt.msg_type == MsgType::BANG)
    {
        std::cout << "  " << Style::s_colors[size_t(MsgType::BANG)] << Style::s_icons[size_t(MsgType::BANG)]
                  << stmt.message;
        return;
    }

    auto it = channels_.find(stmt.channel);
    if (it == channels_.end())
    {
        std::cout << "Channel " << stmt.channel << " does not exist." << std::endl;
        return;
    }

    auto &chan = it->second;

    // check out all sinks subscribed to current channel
    auto &&range = sink_subscriptions_.equal_range(stmt.channel);

    uint8_t required_verbosity = 3 - ((stmt.severity > 3) ? 3 : stmt.severity);
    if (chan.verbosity >= required_verbosity)
    {
        for (auto &&it2 = range.first; it2 != range.second; ++it2)
        {
            if (sinks_.at(it2->second)->is_enabled())
            {
                auto &&sink = sinks_.at(it2->second);
                sink->send(stmt, chan);

                // send backtrace if required
                if (stmt.severity > 1 && backtrace_on_error_)
                {
                    std::string backtrace_log(get_backtrace());
                    sink->send_raw("\033[1;38;2;255;100;0m-------/ "
                                   "\033[1;38;2;255;200;0mBACKTRACE\033[1;38;2;255;100;0m \\-------\n");
                    sink->send_raw("\033[1;38;2;220;220;220m" + backtrace_log +
                                   "\033[1;38;2;255;100;0m---------------------------\n");
                }
            }
        }
    }
}

std::shared_ptr<LogDispatcher> Logger::DISPATCHER = nullptr;

} // namespace log_deprec
} // namespace kb