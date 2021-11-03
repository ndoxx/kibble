#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#define K_DEBUG
#include "event/event_bus.h"

#include <thread>
#include <vector>

using namespace kb;
using namespace kb::event;

// This event cannot be serialized into a stream
struct ExampleEvent
{
    uint32_t first;
    uint32_t second;
};

// This event can be serialized into a stream, as it defines a stream operator
struct StreamableExampleEvent
{
    friend std::ostream &operator<<(std::ostream &, const StreamableExampleEvent &);

    uint32_t first;
    uint32_t second;
};

std::ostream &operator<<(std::ostream &stream, const StreamableExampleEvent &e)
{
    stream << "{first: " << e.first << ", second: " << e.second << '}';
    return stream;
}

// Free function to handle ExampleEvent events
bool handle_event(const ExampleEvent &e)
{
    KLOG("event", 1) << "handle_event(): " << e.first << ' ' << e.second << std::endl;
    return false;
}

// Free function to handle StreamableExampleEvent events
bool handle_streamable_event(const StreamableExampleEvent &e)
{
    KLOG("event", 1) << "handle_streamable_event(): " << e.first << ' ' << e.second << std::endl;
    return false;
}

class ExampleHandler
{
public:
    // Member function to handle StreamableExampleEvent events
    bool handle_streamable_event(const StreamableExampleEvent &e) const
    {
        KLOG("event", 1) << "ExampleHandler::handle_streamable_event(): " << e.first << ' ' << e.second << std::endl;
        return false;
    }

    // Member function to handle ExampleEvent events
    bool handle_event(const ExampleEvent &e)
    {
        KLOG("event", 1) << "ExampleHandler::handle_event(): " << e.first << ' ' << e.second << std::endl;
        return false;
    }
};

struct PokeEvent
{
};

class BasePokeHandler
{
public:
    virtual ~BasePokeHandler() = default;
    virtual bool handle_poke(const PokeEvent &) = 0;
};

class DogHandler : public BasePokeHandler
{
public:
    bool handle_poke(const PokeEvent &) override
    {
        KLOG("event", 1) << "Woof!" << std::endl;
        return false;
    }
};

class CatHandler : public BasePokeHandler
{
public:
    bool handle_poke(const PokeEvent &) override
    {
        KLOG("event", 1) << "Meow." << std::endl;
        return false;
    }
};

auto square(int x) -> int
{
    return x * x;
}

auto cube(int x) -> int
{
    return x * x * x;
}

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("delegate", 3));
    KLOGGER(create_channel("event", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    KLOGN("delegate") << "Using the Delegate class" << std::endl;

    auto d1 = Delegate<int(int)>::create<&square>();
    KLOG("delegate", 1) << d1(2) << std::endl;

    auto str = std::string{"Hello"};
    auto d2 = Delegate<size_t()>::create<&std::string::size>(&str);
    KLOG("delegate", 1) << d2() << std::endl;

    auto d3 = Delegate<void(int)>::create<&std::string::push_back>(&str);
    d3('!');
    KLOG("delegate", 1) << str << std::endl;

    KLOG("delegate", 1) << KF_(kb::col::sienna) << "Checking delegate equality" << std::endl;
    auto d1_2 = Delegate<int(int)>::create<&square>();
    auto d4 = Delegate<int(int)>::create<&cube>();
    KLOG("delegate", 1) << std::boolalpha << (d1 == d1_2) << std::endl;
    KLOG("delegate", 1) << std::boolalpha << (d1 == d4) << std::endl;

    KLOGN("event") << "Using the EventBus class" << std::endl;

    ExampleHandler example_handler;
    EventBus event_bus;

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
    KLOG("event", 1) << KF_(kb::col::sienna) << "Queued events are logged instantly..." << std::endl;
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
    KLOG("event", 1) << KF_(kb::col::sienna) << "... and handled in a deferred fashion" << std::endl;
    event_bus.dispatch();

    // Supports polymorphism
    KLOG("event", 1) << KF_(kb::col::sienna) << "Polymorphism works out of the box" << std::endl;
    std::unique_ptr<BasePokeHandler> a = std::make_unique<DogHandler>();
    std::unique_ptr<BasePokeHandler> b = std::make_unique<CatHandler>();

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
