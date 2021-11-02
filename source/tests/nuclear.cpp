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

template <typename EventT> using EventDelegate = kb::Delegate<bool(const EventT &)>;

template <typename EventT> class EventQueue2 : public kb::event::detail::AbstractEventQueue
{
public:
    /*template <typename ClassT>
    inline void subscribe(ClassT *instance, bool (ClassT::*memberFunction)(const EventT &), uint32_t priority)
    {
        auto d = EventDelegate<EventT>{};
        d.template bind<memberFunction>(instance);
        delegates_.push_back({priority, std::move(d)});
        sort();
    }*/

    template <auto Function,
              typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(Function), const EventT &>>>
    inline void subscribe(uint32_t priority)
    {
        auto d = EventDelegate<EventT>{};
        d.template bind<Function>();
        delegates_.push_back({priority, std::move(d)});
        sort();
    }

    inline void submit(const EventT &event)
    {
        queue_.push(event);
    }
    inline void submit(EventT &&event)
    {
        queue_.push(event);
    }

    inline void fire(const EventT &event) const
    {
        for (auto &&[priority, handler] : delegates_)
            if (handler(event))
                break; // If handler returns true, event is not propagated further
    }

    void process() override final
    {
        while (!queue_.empty())
        {
            for (auto &&[priority, handler] : delegates_)
                if (handler(queue_.front()))
                    break;

            queue_.pop();
        }
    }

    bool empty() const override final
    {
        return queue_.empty();
    }

    size_t size() const override final
    {
        return queue_.size();
    }

private:
    inline void sort()
    {
        // greater_equal generates the desired behavior: for equal priority range of subscribers,
        // latest subscriber handles the event first
        std::sort(delegates_.begin(), delegates_.end(),
                  [](const PriorityDelegate &pd1, const PriorityDelegate &pd2) { return pd1.first >= pd2.first; });
    }

private:
    using PriorityDelegate = std::pair<uint32_t, EventDelegate<EventT>>;
    using DelegateList = std::vector<PriorityDelegate>;
    using Queue = std::queue<EventT>;
    DelegateList delegates_;
    Queue queue_;
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
    /*
        EventBus eb;

        eb.set_event_tracking_predicate([](auto id) {
            (void)id;
            return true;
        });

        eb.subscribe<CollideEventNoStream>(handle_collide_no_stream);
        eb.subscribe<CollideEvent>(handle_collide);

        eb.fire<CollideEventNoStream>({1, 2});
        eb.fire<CollideEvent>({1, 2});
    */

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

    EventQueue2<CollideEvent> q;
    q.subscribe<&handle_collide>(0);

    return 0;
}
