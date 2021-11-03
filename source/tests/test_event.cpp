#include <array>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "event/event_bus.h"
#include "util/delegate.h"
#include <catch2/catch_all.hpp>

using namespace kb;
using namespace kb::event;

// I also test delegates here, why not.

auto square(int x) -> int
{
    return x * x;
}

auto cube(int x) -> int
{
    return x * x * x;
}

TEST_CASE("It is possible to delegate a free function", "[delegate]")
{
    auto d = Delegate<int(int)>::create<&square>();
    REQUIRE(d(2) == 4);
    REQUIRE(d(5) == 25);
}

TEST_CASE("It is possible to delegate a non-mutating member function", "[delegate]")
{
    auto str = std::string{"Hello"};
    auto d = Delegate<size_t()>::create<&std::string::size>(&str);
    REQUIRE(d() == 5);
}

TEST_CASE("It is possible to delegate a mutating member function", "[delegate]")
{
    auto str = std::string{"Hello"};
    auto d = Delegate<void(int)>::create<&std::string::push_back>(&str);
    d('!');
    REQUIRE(str.compare("Hello!") == 0);
}

TEST_CASE("Free delegate comparison should be reflexive", "[delegate]")
{
    auto d = Delegate<int(int)>::create<&square>();
    REQUIRE(d == d);
    REQUIRE_FALSE(d != d);
}

TEST_CASE("A delegate should be equal to another delegate pointing to the same free function", "[delegate]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    [[maybe_unused]] auto d3 = Delegate<int(int)>::create<&cube>();
    auto d2 = Delegate<int(int)>::create<&square>();
    REQUIRE(d1 == d2);
    REQUIRE_FALSE(d1 != d2);
}

TEST_CASE("A delegate should not be equal to another delegate pointing to a different free function", "[delegate]")
{
    auto d1 = Delegate<int(int)>::create<&square>();
    [[maybe_unused]] auto d3 = Delegate<int(int)>::create<&square>();
    auto d2 = Delegate<int(int)>::create<&cube>();
    REQUIRE_FALSE(d1 == d2);
    REQUIRE(d1 != d2);
}

TEST_CASE("Member delegate comparison should be reflexive", "[delegate]")
{
    auto str = std::string{"Hello"};
    auto d1 = Delegate<size_t()>::create<&std::string::size>(&str);
    auto d2 = Delegate<size_t()>::create<&std::string::size>(&str);

    REQUIRE(d1 == d2);
    REQUIRE_FALSE(d1 != d2);
}

TEST_CASE("Member delegates with different instances should be different", "[delegate]")
{
    auto str1 = std::string{"Hello"};
    auto str2 = std::string{"World"};
    auto d1 = Delegate<size_t()>::create<&std::string::size>(&str1);
    auto d2 = Delegate<size_t()>::create<&std::string::size>(&str2);

    REQUIRE_FALSE(d1 == d2);
    REQUIRE(d1 != d2);
}

TEST_CASE("Member delegates with different member pointers should be different", "[delegate]")
{
    auto str = std::string{"Hello"};
    // Here we need to have the same signature otherwise it wouldn't even compile (and we would
    // know at compile-time that they are different)
    auto d1 = Delegate<size_t()>::create<&std::string::size>(&str);
    auto d2 = Delegate<size_t()>::create<&std::string::length>(&str);

    REQUIRE_FALSE(d1 == d2);
    REQUIRE(d1 != d2);
}

struct CollideEvent
{
    uint32_t first;
    uint32_t second;
};

struct DummyEvent
{
};

struct UnhandledEvent
{
    int a;
};

bool handle_dummy(const DummyEvent &)
{
    return false;
}

class CollisionResponseSystem
{
public:
    bool on_collision(const CollideEvent &event)
    {
        handled.push_back(std::pair(event.first, event.second));
        return false;
    }

    std::vector<std::pair<uint32_t, uint32_t>> handled;
};

class EventFixture
{
public:
    EventFixture()
    {
        event_bus.subscribe<&CollisionResponseSystem::on_collision>(collision_response);
        event_bus.subscribe<&handle_dummy>();
    }

protected:
    CollisionResponseSystem collision_response;
    EventBus event_bus;
};

TEST_CASE_METHOD(EventFixture, "Events fired instantly should be handled immediately", "[evt]")
{
    event_bus.fire<CollideEvent>({0, 1});

    REQUIRE(event_bus.empty());
    REQUIRE(collision_response.handled.size() == 1);
    auto &&[a, b] = collision_response.handled[0];
    REQUIRE((a == 0 && b == 1));
}

TEST_CASE_METHOD(EventFixture, "Enqueued events should not be processed before a call to dispatch", "[evt]")
{
    event_bus.enqueue<CollideEvent>({0, 1});
    REQUIRE(collision_response.handled.size() == 0);

    event_bus.dispatch();

    REQUIRE(collision_response.handled.size() == 1);
    auto &&[a, b] = collision_response.handled[0];
    REQUIRE((a == 0 && b == 1));
}

TEST_CASE_METHOD(EventFixture, "Enqueueing multiple events should work", "[evt]")
{
    event_bus.enqueue<CollideEvent>({0, 1});
    event_bus.enqueue<CollideEvent>({2, 3});
    REQUIRE(collision_response.handled.size() == 0);

    event_bus.dispatch();

    REQUIRE(collision_response.handled.size() == 2);
    auto &&[a, b] = collision_response.handled[0];
    REQUIRE((a == 0 && b == 1));
    auto &&[c, d] = collision_response.handled[1];
    REQUIRE((c == 2 && d == 3));
}

TEST_CASE_METHOD(EventFixture, "Enqueueing an unhandled event does nothing", "[evt]")
{
    event_bus.enqueue<UnhandledEvent>({0});
    REQUIRE(event_bus.empty());
}

TEST_CASE_METHOD(EventFixture, "Unprocessed event count can be queried", "[evt]")
{
    event_bus.enqueue<DummyEvent>({});
    event_bus.enqueue<CollideEvent>({0, 1});
    event_bus.enqueue<CollideEvent>({2, 3});
    REQUIRE(event_bus.get_unprocessed_count() == 3);
}

TEST_CASE_METHOD(EventFixture, "Events can be dropped selectively", "[evt]")
{
    event_bus.enqueue<DummyEvent>({});
    event_bus.enqueue<CollideEvent>({0, 1});
    event_bus.enqueue<CollideEvent>({2, 3});

    event_bus.drop<DummyEvent>();
    REQUIRE(event_bus.get_unprocessed_count() == 2);
}

TEST_CASE_METHOD(EventFixture, "All events can be dropped at the same time", "[evt]")
{
    event_bus.enqueue<DummyEvent>({});
    event_bus.enqueue<CollideEvent>({0, 1});
    event_bus.enqueue<CollideEvent>({2, 3});

    event_bus.drop();
    REQUIRE(event_bus.get_unprocessed_count() == 0);
}

struct EventA
{
    int a;
};

struct EventB
{
    int b;
};

bool handle_A(const EventA &)
{
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1ms);
    return false;
}

bool handle_B(const EventB &)
{
    return false;
}

class TimeoutFixture
{
public:
    TimeoutFixture()
    {
        event_bus.subscribe<&handle_A>();
        event_bus.subscribe<&handle_B>();
    }

protected:
    EventBus event_bus;
};

TEST_CASE_METHOD(TimeoutFixture, "Event dispatching can timeout, events should wait for next dispatch", "[timeout]")
{
    using namespace std::chrono_literals;

    // EventA takes 1ms to handle
    event_bus.enqueue<EventA>({0});
    event_bus.enqueue<EventA>({1});
    event_bus.enqueue<EventB>({0});

    bool done = event_bus.dispatch(1ms);
    REQUIRE_FALSE(done);
    REQUIRE(event_bus.get_unprocessed_count() > 0);

    done = event_bus.dispatch();
    REQUIRE(done);
}

class BaseDummyHandler
{
public:
    virtual ~BaseDummyHandler() = default;
    virtual bool handle_dummy(const DummyEvent &) = 0;

    bool handled = false;
};

class DummyHandlerA : public BaseDummyHandler
{
public:
    bool handle_dummy(const DummyEvent &) override
    {
        handled = true;
        return false;
    }
};

class DummyHandlerB : public BaseDummyHandler
{
public:
    bool handle_dummy(const DummyEvent &) override
    {
        handled = true;
        return false;
    }
};

class PolyFixture
{
public:
    PolyFixture()
    {
        pa = std::make_unique<DummyHandlerA>();
        pb = std::make_unique<DummyHandlerB>();
        event_bus.subscribe<&BaseDummyHandler::handle_dummy>(*pa);
        event_bus.subscribe<&BaseDummyHandler::handle_dummy>(*pb);
    }

protected:
    EventBus event_bus;
    std::unique_ptr<BaseDummyHandler> pa;
    std::unique_ptr<BaseDummyHandler> pb;
};

TEST_CASE_METHOD(PolyFixture, "Subscribing a virtual member should work", "[poly]")
{
    event_bus.fire<DummyEvent>({});
    REQUIRE(pa->handled);
    REQUIRE(pb->handled);
}

struct PokeEvent
{
};
struct PokeHandler
{
    bool handle_poke(const PokeEvent &)
    {
        ++handle_count;
        return false;
    }
    size_t handle_count = 0;
};

static size_t s_handle_count_1 = 0;
static size_t s_handle_count_2 = 0;

bool handle_poke_1(const PokeEvent &)
{
    ++s_handle_count_1;
    return false;
}

bool handle_poke_2(const PokeEvent &)
{
    ++s_handle_count_2;
    return false;
}

bool fake_handle_poke(const PokeEvent &)
{
    return false;
}

class UnsubFixture
{
public:
    UnsubFixture()
    {
        s_handle_count_1 = 0;
        s_handle_count_2 = 0;
        event_bus.subscribe<&PokeHandler::handle_poke>(h1);
        event_bus.subscribe<&PokeHandler::handle_poke>(h2);
        event_bus.subscribe<&handle_poke_1>();
        event_bus.subscribe<&handle_poke_2>();
    }

protected:
    PokeHandler h1;
    PokeHandler h2;
    EventBus event_bus;
};

TEST_CASE_METHOD(UnsubFixture, "Unsubscribing free functions should work", "[unsub]")
{
    bool success = event_bus.unsubscribe<&handle_poke_1>();
    event_bus.fire<PokeEvent>({});

    REQUIRE(success);
    REQUIRE(s_handle_count_1 == 0);
    // Testing for side effects
    REQUIRE(s_handle_count_2 == 1);
    REQUIRE(h1.handle_count == 1);
    REQUIRE(h2.handle_count == 1);
}

TEST_CASE_METHOD(UnsubFixture, "Unsubscribing member functions should work", "[unsub]")
{
    bool success = event_bus.unsubscribe<&PokeHandler::handle_poke>(h1);
    event_bus.fire<PokeEvent>({});

    REQUIRE(success);
    REQUIRE(h1.handle_count == 0);
    // Testing for side effects
    REQUIRE(s_handle_count_1 == 1);
    REQUIRE(s_handle_count_2 == 1);
    REQUIRE(h2.handle_count == 1);
}

TEST_CASE_METHOD(UnsubFixture, "Unsubscribing non-existent subscriber should do nothing", "[unsub]")
{
    bool success = event_bus.unsubscribe<&fake_handle_poke>();
    event_bus.fire<PokeEvent>({});

    REQUIRE_FALSE(success);
    // Testing for side effects
    REQUIRE(s_handle_count_1 == 1);
    REQUIRE(s_handle_count_2 == 1);
    REQUIRE(h1.handle_count == 1);
    REQUIRE(h2.handle_count == 1);
}