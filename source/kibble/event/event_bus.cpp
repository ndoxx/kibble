#include "kibble/event/event_bus.h"

#include <numeric>

namespace kb::event
{

static constexpr auto k_timepoint_max = std::chrono::time_point<std::chrono::high_resolution_clock>::max();

bool EventBus::dispatch(std::chrono::nanoseconds timeout)
{
    bool enable_timeout = timeout.count() > 0;
    auto deadline = enable_timeout ? nanoClock::now() + timeout : k_timepoint_max;
    // An event once handled can cause another event to be enqueued, so
    // we iterate till all events have been processed
    while (!empty())
    {
        for (const auto& [id, queue] : event_queues_)
        {
            if (!queue->process(deadline) && enable_timeout)
            {
                return false;
            }
        }
    }

    return true;
}

void EventBus::drop()
{
    for (auto&& [id, queue] : event_queues_)
    {
        queue->drop();
    }
}

bool EventBus::empty()
{
    for (auto&& [id, queue] : event_queues_)
    {
        if (!queue->empty())
        {
            return false;
        }
    }

    return true;
}

size_t EventBus::get_unprocessed_count()
{
    return std::accumulate(event_queues_.begin(), event_queues_.end(), 0u, [](size_t accumulator, auto&& entry) {
        return accumulator + (entry.second ? entry.second->size() : 0u);
    });
}

} // namespace kb::event