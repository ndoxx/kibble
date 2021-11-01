#include "event_bus.h"

namespace kb
{
namespace event
{

void EventBus::dispatch()
{
    // An event once handled can cause another event to be enqueued, so
    // we iterate till all events have been processed
    while (!empty())
        for (auto &&[id, queue] : event_queues_)
            queue->process();
}

bool EventBus::empty()
{
    for (auto &&[id, queue] : event_queues_)
        if (!queue->empty())
            return false;

    return true;
}

size_t EventBus::get_unprocessed_count()
{
    return std::accumulate(event_queues_.begin(), event_queues_.end(), 0u, [](size_t accumulator, auto &&entry) {
        return accumulator + (entry.second ? entry.second->size() : 0u);
    });
}

} // namespace event
} // namespace kb