#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#define K_DEBUG
#include "event/event_bus.h"

#include <vector>

using namespace kb;
using namespace kb::event;

// This event cannot be serialized into a stream
struct CollideEventNoStream
{
    uint32_t first;
    uint32_t second;
};

// This event can be serialized into a stream, as it defines a stream operator
struct CollideEvent
{
    friend std::ostream &operator<<(std::ostream &, const CollideEvent &);

    uint32_t first;
    uint32_t second;
};

std::ostream &operator<<(std::ostream &stream, const CollideEvent &e)
{
    stream << "{first: " << e.first << ", second: " << e.second << '}';
    return stream;
}

// Free function to handle CollideEventNoStream events
bool handle_collide_no_stream(const CollideEventNoStream &e)
{
    KLOG("event", 1) << "handle_collide_no_stream(): " << e.first << ' ' << e.second << std::endl;
    return false;
}

// Free function to handle CollideEvent events
bool handle_collide(const CollideEvent &e)
{
    KLOG("event", 1) << "handle_collide(): " << e.first << ' ' << e.second << std::endl;
    return false;
}

class CollisionHandler
{
public:
    // Member function to handle CollideEvent events
    bool handle_collide(const CollideEvent &e) const
    {
        KLOG("event", 1) << "CollisionHandler::handle_collide(): " << e.first << ' ' << e.second << std::endl;
        return false;
    }

    // Member function to handle CollideEventNoStream events
    bool handle_collide_nostream(const CollideEventNoStream &e)
    {
        KLOG("event", 1) << "CollisionHandler::handle_collide_nostream(): " << e.first << ' ' << e.second << std::endl;
        return false;
    }
};

auto square(int x) -> int
{
    return x * x;
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

    auto d1 = Delegate<int(int)>{};
    d1.bind<&square>();
    KLOG("delegate", 1) << d1(2) << std::endl;

    auto str = std::string{"Hello"};
    auto d2 = Delegate<size_t()>{};
    d2.bind<&std::string::size>(&str);
    KLOG("delegate", 1) << d2() << std::endl;

    auto d3 = Delegate<void(int)>{};
    d3.bind<&std::string::push_back>(&str);
    d3('!');
    KLOG("delegate", 1) << str << std::endl;

    KLOGN("event") << "Using the EventBus class" << std::endl;

    CollisionHandler ch;
    EventBus eb;

    eb.set_event_tracking_predicate([](auto id) {
        (void)id;
        return true;
    });

    // Register a free function
    eb.subscribe<&handle_collide_no_stream>();
    // Register a non-const member function
    eb.subscribe<&CollisionHandler::handle_collide_nostream>(&ch);
    // Register a const member function
    eb.subscribe<&CollisionHandler::handle_collide>(&ch);
    // Register a free function
    eb.subscribe<&handle_collide>();

    eb.fire<CollideEventNoStream>({1, 2});
    eb.fire<CollideEvent>({1, 2});

    return 0;
}
