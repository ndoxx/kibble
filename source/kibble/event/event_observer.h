#pragma once

#include <cstdint>
#include <string_view>

namespace kb::event
{

struct EventInfo
{
    using EventID = uint64_t;
    enum class Phase : uint8_t
    {
        Fire,
        Enqueue,
        Dispatch,
        Handle
    };

    const void* event_ptr;      ///< Type-erased pointer to the actual event
    EventID type_id;            ///< Event type hash
    std::string_view type_name; ///< Reflected event type name
    Phase phase;
};

class EventObserver
{
public:
    virtual ~EventObserver() = default;
    virtual void on_event(const EventInfo& info) = 0;
};

} // namespace kb::event