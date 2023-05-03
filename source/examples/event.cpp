#define K_DEBUG
#include "event/event_bus.h"
#include "logger2/formatters/vscode_terminal_formatter.h"
#include "logger2/logger.h"
#include "logger2/sinks/console_sink.h"
#include "math/color_table.h"

#include <iostream>
#include <thread>
#include <vector>

using namespace kb;
using namespace kb::event;
using namespace kb::log;

// This event cannot be serialized into a stream
struct ExampleEvent
{
    uint32_t first;
    uint32_t second;
};

// This event can be serialized into a stream, as it defines a stream operator
struct StreamableExampleEvent
{
    friend std::ostream& operator<<(std::ostream&, const StreamableExampleEvent&);

    uint32_t first;
    uint32_t second;
};

std::ostream& operator<<(std::ostream& stream, const StreamableExampleEvent& e)
{
    stream << "{first: " << e.first << ", second: " << e.second << '}';
    return stream;
}

// Free function to handle ExampleEvent events
bool handle_event(const ExampleEvent& e)
{
    std::cout << "handle_event(): " << e.first << ' ' << e.second << std::endl;
    return false;
}

// Free function to handle StreamableExampleEvent events
bool handle_streamable_event(const StreamableExampleEvent& e)
{
    std::cout << "handle_streamable_event(): " << e.first << ' ' << e.second << std::endl;
    return false;
}

class ExampleHandler
{
public:
    ExampleHandler(const kb::log::Channel& log_channel) : log_channel_(log_channel)
    {
    }

    // Member function to handle StreamableExampleEvent events
    bool handle_streamable_event(const StreamableExampleEvent& e) const
    {
        klog(log_channel_).uid("ExampleHandler::handle_streamable_event()").info("{} {}", e.first, e.second);
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
    event_bus.set_logger_channel(&chan_event);

    // Track all events
    event_bus.set_event_tracking_predicate([](auto id) {
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
    event_bus.subscribe<&ExampleHandler::handle_streamable_event>(example_handler, 1);
    // Register a free function
    event_bus.subscribe<&handle_streamable_event>();

    // Enqueue events
    klog(chan_kibble).info("Queued events are logged instantly...");
    // When an event is enqueued, the logging information will show a [q] flag before the event
    // name, and the label color will be turquoise.
    // This event does not define a stream operator, the logging information will only
    // show a label with the event name.
    event_bus.enqueue<ExampleEvent>({1, 2});
    // This event defines a stream operator, it will be serialized to the stream when the event
    // gets logged, displaying "{first: 1, second: 2}" next to the event label.
    event_bus.enqueue<StreamableExampleEvent>({1, 2});

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
