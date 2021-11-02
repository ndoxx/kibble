#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"

#define K_DEBUG
#include "event/event_bus.h"

#include "util/delegate.h"

#include <vector>

using namespace kb;
using namespace kb::event;

struct CollideEventNoStream
{
    uint32_t first;
    uint32_t second;
};

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

bool handle_collide_no_stream(const CollideEventNoStream &e)
{
    KLOG("event", 1) << "handle_collide_no_stream(): " << e.first << ' ' << e.second << std::endl;
    return false;
}

bool handle_collide(const CollideEvent &e)
{
    KLOG("event", 1) << "handle_collide(): " << e.first << ' ' << e.second << std::endl;
    return false;
}

class CollisionHandler
{
public:
    bool handle_collide(const CollideEvent &e) const
    {
        KLOG("event", 1) << "CollisionHandler::handle_collide(): " << e.first << ' ' << e.second << std::endl;
        return false;
    }

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

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("event", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    CollisionHandler ch;
    EventBus eb;

    eb.set_event_tracking_predicate([](auto id) {
        (void)id;
        return true;
    });

    eb.subscribe<&handle_collide_no_stream>();
    eb.subscribe<&CollisionHandler::handle_collide_nostream>(&ch);
    eb.subscribe<&CollisionHandler::handle_collide>(&ch);
    eb.subscribe<&handle_collide>();

    eb.fire<CollideEventNoStream>({1, 2});
    eb.fire<CollideEvent>({1, 2});

    /*
        auto d1 = Delegate<int(int)>{};
        d1.bind<&square>();
        KLOG("nuclear", 1) << d1(2) << std::endl;

        auto str = std::string{"Hello"};
        auto d2 = Delegate<size_t()>{};
        d2.bind<&std::string::size>(&str);
        KLOG("nuclear", 1) << d2() << std::endl;

        auto d3 = Delegate<void(int)>{};
        d3.bind<&std::string::push_back>(&str);
        d3('!');
        KLOG("nuclear", 1) << str << std::endl;
    */

    return 0;
}
