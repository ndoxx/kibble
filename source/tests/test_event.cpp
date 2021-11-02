#include <array>
#include <iostream>
#include <random>
#include <vector>

#include "event/event_bus.h"
#include "util/delegate.h"
#include <catch2/catch_all.hpp>

using namespace kb::event;

struct SubscriberPriorityKey
{
    uint16_t flags;
    uint8_t layer_id;
    uint8_t system_id;

    static constexpr uint32_t k_flags_shift = 32u - 16u;
    static constexpr uint32_t k_layer_shift = k_flags_shift - 8u;
    static constexpr uint32_t k_system_shift = k_layer_shift - 8u;
    static constexpr uint32_t k_flags_mask = uint32_t(0x0000ffff) << k_flags_shift;
    static constexpr uint32_t k_layer_mask = uint32_t(0x000000ff) << k_layer_shift;
    static constexpr uint32_t k_system_mask = uint32_t(0x000000ff) << k_system_shift;

    SubscriberPriorityKey() : flags(0), layer_id(0), system_id(0)
    {
    }

    SubscriberPriorityKey(uint8_t _layer_id, uint8_t _system_id = 0, uint16_t _flags = 0)
        : flags(_flags), layer_id(_layer_id), system_id(_system_id)
    {
    }

    inline uint32_t encode()
    {
        return (uint32_t(flags) << k_flags_shift) | (uint32_t(layer_id) << k_layer_shift) |
               (uint32_t(system_id) << k_system_shift);
    }

    inline void decode(uint32_t priority)
    {
        flags = uint16_t((priority & k_flags_mask) >> k_flags_shift);
        layer_id = uint8_t((priority & k_layer_mask) >> k_layer_shift);
        system_id = uint8_t((priority & k_system_mask) >> k_system_shift);
    }
};

[[maybe_unused]] static inline uint32_t subscriber_priority(uint8_t layer_id, uint8_t system_id = 0u,
                                                            uint16_t flags = 0u)
{
    return SubscriberPriorityKey(layer_id, system_id, flags).encode();
}

struct CollideEvent
{
    friend std::ostream &operator<<(std::ostream &, const CollideEvent &);

    uint32_t first;
    uint32_t second;
};

std::ostream &operator<<(std::ostream &stream, const CollideEvent &e)
{
    stream << "first: " << e.first << " second: " << e.second;
    return stream;
}

class ColliderSystem
{
public:
    void fire_collision_instant(EventBus &eb, uint32_t first, uint32_t second)
    {
        eb.fire<CollideEvent>({first, second});
    }

    void enqueue_collision(EventBus &eb, uint32_t first, uint32_t second)
    {
        eb.enqueue<CollideEvent>({first, second});
    }
};

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
        event_bus.subscribe<&CollisionResponseSystem::on_collision>(&collision_response);
    }

protected:
    ColliderSystem collider;
    CollisionResponseSystem collision_response;
    EventBus event_bus;
};

TEST_CASE_METHOD(EventFixture, "Firing event instantly", "[evt]")
{
    collider.fire_collision_instant(event_bus, 0, 1);

    REQUIRE(event_bus.empty());
    REQUIRE(collision_response.handled.size() == 1);
    auto &&[a, b] = collision_response.handled[0];
    REQUIRE((a == 0 && b == 1));
}

TEST_CASE_METHOD(EventFixture, "Enqueueing event", "[evt]")
{
    collider.enqueue_collision(event_bus, 0, 1);
    REQUIRE(collision_response.handled.size() == 0);

    event_bus.dispatch();

    REQUIRE(collision_response.handled.size() == 1);
    auto &&[a, b] = collision_response.handled[0];
    REQUIRE((a == 0 && b == 1));
}

TEST_CASE_METHOD(EventFixture, "Enqueueing multiple events", "[evt]")
{
    collider.enqueue_collision(event_bus, 0, 1);
    collider.enqueue_collision(event_bus, 2, 3);
    REQUIRE(collision_response.handled.size() == 0);

    event_bus.dispatch();

    REQUIRE(collision_response.handled.size() == 2);
    auto &&[a, b] = collision_response.handled[0];
    REQUIRE((a == 0 && b == 1));
    auto &&[c, d] = collision_response.handled[1];
    REQUIRE((c == 2 && d == 3));
}
