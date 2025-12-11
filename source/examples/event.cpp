#ifndef K_DEBUG
#define K_DEBUG
#endif

#include "kibble/event/event_bus.h"
#include "kibble/event/event_observer.h"
#include "kibble/logger/formatters/vscode_terminal_formatter.h"
#include "kibble/logger/logger.h"
#include "kibble/logger/sinks/console_sink.h"
#include "kibble/math/color_table.h"

#include <thread>

using namespace kb;
using namespace kb::event;
using namespace kb::log;

// This event cannot be serialized into a stream
struct ExampleEvent
{
    uint32_t first;
    uint32_t second;
};

// This event can be serialized into a stream, as it defines a formatter
struct FormattableEvent
{
    uint32_t first;
    uint32_t second;
};

template <>
struct fmt::formatter<FormattableEvent>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const FormattableEvent& fe, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "[{}, {}]", fe.first, fe.second);
    }
};

// Logging observer implementation (inline for this example)
template <typename EventT>
concept Formattable = requires(EventT) { fmt::formatter<EventT>{}; };

class LoggingEventObserver : public EventObserver
{
public:
    LoggingEventObserver(const kb::log::Channel* channel) : log_channel_(channel)
    {
    }

    template <typename EventT>
    void register_formatter()
    {
        formatters_[kb::ctti::type_id<EventT>()] = [](const void* ptr) -> std::string {
            const auto& event = *static_cast<const EventT*>(ptr);
            if constexpr (Formattable<EventT>)
            {
                return fmt::format("{}", event);
            }
            else
            {
                return "";
            }
        };
    }

    void on_event(const EventInfo& info) override
    {
        if (!log_channel_ || !should_track_(info.type_id))
        {
            return;
        }

        char phase_char = phase_to_char(info.phase);

        auto it = formatters_.find(info.type_id);
        if (it != formatters_.end() && !it->second(info.event_ptr).empty())
        {
            klog(log_channel_).debug("[{}] {}: {}", phase_char, info.type_name, it->second(info.event_ptr));
        }
        else
        {
            klog(log_channel_).debug("[{}] {}", phase_char, info.type_name);
        }
    }

    void set_filter(std::function<bool(EventID)> filter)
    {
        should_track_ = std::move(filter);
    }

private:
    static char phase_to_char(EventInfo::Phase phase)
    {
        using enum EventInfo::Phase;
        switch (phase)
        {
        case Fire:
            return 'f';
        case Enqueue:
            return 'q';
        case Dispatch:
            return 'd';
        case Handle:
            return 'h';
        default:
            return '?';
        }
    }

    const kb::log::Channel* log_channel_;
    std::function<bool(EventID)> should_track_ = [](EventID) { return false; };
    std::unordered_map<EventID, std::function<std::string(const void*)>> formatters_;
};

// Free function to handle ExampleEvent events
bool handle_event(const ExampleEvent& e)
{
    fmt::println("handle_event(): {} {}", e.first, e.second);
    return false;
}

// Free function to handle FormattableEvent events
bool handle_formattable_event(const FormattableEvent& e)
{
    fmt::println("handle_formattable_event(): {} ", e);
    return false;
}

class ExampleHandler
{
public:
    ExampleHandler(const kb::log::Channel& log_channel) : log_channel_(log_channel)
    {
    }

    // Member function to handle FormattableEvent events
    bool handle_formattable_event(const FormattableEvent& e) const
    {
        klog(log_channel_).uid("ExampleHandler::handle_formattable_event()").info("{}", e);
        return false;
    }

    // Member function to handle ExampleEvent events
    bool handle_event(const ExampleEvent& e)
    {
        klog(log_channel_).uid("ExampleHandler::handle_event()").info("{} {}", e.first, e.second);
        return false;
    }

private:
    const kb::log::Channel& log_channel_;
};

struct PokeEvent
{
};

class BasePokeHandler
{
public:
    virtual ~BasePokeHandler() = default;
    virtual bool handle_poke(const PokeEvent&) = 0;
};

class DogHandler : public BasePokeHandler
{
public:
    DogHandler(const kb::log::Channel& log_channel) : log_channel_(log_channel)
    {
    }

    bool handle_poke(const PokeEvent&) override
    {
        klog(log_channel_).uid("DogHandler").info("Woof!");
        return false;
    }

private:
    const kb::log::Channel& log_channel_;
};

class CatHandler : public BasePokeHandler
{
public:
    CatHandler(const kb::log::Channel& log_channel) : log_channel_(log_channel)
    {
    }

    bool handle_poke(const PokeEvent&) override
    {
        klog(log_channel_).uid("CatHandler").info("Meow.");
        return false;
    }

private:
    const kb::log::Channel& log_channel_;
};

auto square(int x) -> int
{
    return x * x;
}

auto cube(int x) -> int
{
    return x * x * x;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    auto console_formatter = std::make_shared<VSCodeTerminalFormatter>();
    auto console_sink = std::make_shared<ConsoleSink>();
    console_sink->set_formatter(console_formatter);
    Channel chan_kibble(Severity::Verbose, "kibble", "kib", kb::col::aliceblue);
    chan_kibble.attach_sink(console_sink);
    Channel chan_handler(Severity::Verbose, "handler", "hnd", kb::col::darkorange);
    chan_handler.attach_sink(console_sink);
    Channel chan_event(Severity::Verbose, "event", "evt", kb::col::turquoise);
    chan_event.attach_sink(console_sink);

    klog(chan_kibble).info("Using the Delegate class");
    auto d1 = Delegate<int(int)>::create<&square>();
    klog(chan_kibble).verbose("{}", d1(2));

    auto str = std::string{"Hello"};
    auto d2 = Delegate<size_t()>::create<&std::string::size>(&str);
    klog(chan_kibble).verbose("{}", d2());

    auto d3 = Delegate<void(int)>::create<&std::string::push_back>(&str);
    d3('!');
    klog(chan_kibble).verbose(str);

    klog(chan_kibble).info("Checking delegate equality");
    auto d1_2 = Delegate<int(int)>::create<&square>();
    auto d4 = Delegate<int(int)>::create<&cube>();
    klog(chan_kibble).verbose("d1 == d1_2: {}", (d1 == d1_2));
    klog(chan_kibble).verbose("d1 == d4: {}", (d1 == d4));

    klog(chan_kibble).info("Using the EventBus class");

    ExampleHandler example_handler(chan_handler);
    EventBus event_bus;

    // Create and configure the logging observer
    auto* observer = event_bus.create_observer<LoggingEventObserver>(&chan_event);

    // Register formatters for events that support formatting
    observer->register_formatter<FormattableEvent>();
    observer->register_formatter<ExampleEvent>();
    observer->register_formatter<PokeEvent>();

    // Track all events
    observer->set_filter([](auto id) {
        (void)id;
        return true;
    });

    // Register a free function
    // The event type is automagically deduced
    event_bus.subscribe<&handle_event>();
    // Register a non-const member function
    // This subscriber will execute first, as it was added last
    event_bus.subscribe<&ExampleHandler::handle_event>(example_handler);

    // Register a const member function
    // This subscriber will execute first, as it has a higher priority
    event_bus.subscribe<&ExampleHandler::handle_formattable_event>(example_handler, 1);
    // Register a free function
    event_bus.subscribe<&handle_formattable_event>();

    // Enqueue events
    klog(chan_kibble).info("Queued events are logged instantly...");
    // When an event is enqueued, the logging information will show a [q] flag before the event
    // name, and the label color will be turquoise.
    // This event does not define a formatter, the logging information will only
    // show a label with the event name.
    event_bus.enqueue<ExampleEvent>({1, 2});
    // This event defines a formatter, it will be serialized when the event
    // gets logged, displaying "[1, 2]" next to the event label.
    event_bus.enqueue<FormattableEvent>({1, 2});

    // Wait a bit
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(500ms);

    // Dispatch all events
    klog(chan_kibble).info("... and handled in a deferred fashion");
    event_bus.dispatch();

    // Supports polymorphism
    klog(chan_kibble).info("Polymorphism works out of the box");
    std::unique_ptr<BasePokeHandler> a = std::make_unique<DogHandler>(chan_handler);
    std::unique_ptr<BasePokeHandler> b = std::make_unique<CatHandler>(chan_handler);

    // Two specialized handlers register the base class function
    event_bus.subscribe<&BasePokeHandler::handle_poke>(*a);
    event_bus.subscribe<&BasePokeHandler::handle_poke>(*b);

    // The PokeEvent is an example of a "tag event", which contains no data.
    // This event will be fired, and will trigger an immediate response, without the need to dispatch().
    // When an event is fired, the logging information will show a [f] flag before the event
    // name, and the label color will be mustard color.
    event_bus.fire<PokeEvent>({});

    return 0;
}