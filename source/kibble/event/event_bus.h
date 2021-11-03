/*
 * This is a rewrite of the event system used in my projects WCore and ErwinEngine
 * Initially inspired by https://medium.com/@savas/nomad-game-engine-part-7-the-event-system-45a809ccb68f
 * Differences are:
 * - Using const references or rvalues instead of pointers to pass events around
 * - Using my constexpr ctti::type_id<>() instead of RTTI
 * - Leak free
 * - Events can be any type, no need to derive from a base event class
 * - Deferred event handling with event queues, instant firing still supported
 * - Priority mechanism
 * - Zero-cost delegates
 * - Several additional features
 *
 * TODO:
 * [ ] Dispatch deadlock detection?
 * [ ] Event pooling if deemed necessary
 */

#pragma once
#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <queue>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../ctti/ctti.h"
#include "../logger/logger.h"
#include "../time/clock.h"
#include "../util/delegate.h"

namespace kb
{
namespace event
{

namespace detail
{

template <typename T>
concept Streamable = requires(std::ostream &stream, T a)
{
    stream << a;
};

template <typename EventT>
using EventDelegate = kb::Delegate<bool(const EventT &)>;

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds>;

template <typename T>
using to_pointer = std::add_pointer<typename std::remove_reference<T>::type>;

// Interface for an event queue
class AbstractEventQueue
{
public:
    virtual ~AbstractEventQueue() = default;
    virtual bool process(TimePoint) = 0;
    virtual void drop() = 0;
    virtual bool empty() const = 0;
    virtual size_t size() const = 0;
};

// Concrete event queue, can subscribe functions, and process events immediately or in a deferred fashion
template <typename EventT>
class EventQueue : public kb::event::detail::AbstractEventQueue
{
public:
    template <std::invocable<const EventT &> auto Function>
    inline void subscribe(uint32_t priority)
    {
        delegates_.emplace_back(priority, EventDelegate<EventT>::template create<Function>());
        sort();
    }

    // I use template magic to handle both const and non-const cases at the same time
    template <typename ClassRef,
              std::invocable<typename to_pointer<ClassRef>::type, const EventT &> auto MemberFunction>
    inline void subscribe(typename to_pointer<ClassRef>::type instance, uint32_t priority = 0u)
    {
        delegates_.emplace_back(priority, EventDelegate<EventT>::template create<MemberFunction>(instance));
        sort();
    }

    template <std::invocable<const EventT &> auto Function>
    bool unsubscribe()
    {
        auto target = EventDelegate<EventT>::template create<Function>();
        auto findit =
            std::find_if(delegates_.begin(), delegates_.end(), [&target](const auto &p) { return p.second == target; });
        if (findit != delegates_.end())
        {
            delegates_.erase(findit);
            return true;
        }
        return false;
    }

    // I use template magic to handle both const and non-const cases at the same time
    template <typename ClassRef,
              std::invocable<typename to_pointer<ClassRef>::type, const EventT &> auto MemberFunction>
    bool unsubscribe(typename to_pointer<ClassRef>::type instance)
    {
        auto target = EventDelegate<EventT>::template create<MemberFunction>(instance);
        auto findit =
            std::find_if(delegates_.begin(), delegates_.end(), [&target](const auto &p) { return p.second == target; });
        if (findit != delegates_.end())
        {
            delegates_.erase(findit);
            return true;
        }
        return false;
    }

    inline void submit(const EventT &event)
    {
        queue_.push(event);
    }
    inline void submit(EventT &&event)
    {
        queue_.push(event);
    }

    void fire(const EventT &event) const
    {
        // Iterate in reverse order, so the last subscribers execute first
        for (auto it = delegates_.rbegin(); it != delegates_.rend(); ++it)
            if (it->second(event))
                break; // If handler returns true, event is not propagated further
    }

    bool process(TimePoint deadline) override final
    {
        while (!queue_.empty())
        {
            for (auto it = delegates_.rbegin(); it != delegates_.rend(); ++it)
                if (it->second(queue_.front()))
                    break;

            queue_.pop();

            // Timeout if the deadline was exceeded
            if (nanoClock::now() > deadline)
                return false;
        }

        return true;
    }

    void drop() override final
    {
        Queue{}.swap(queue_);
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
        std::sort(delegates_.begin(), delegates_.end(),
                  [](const PriorityDelegate &pd1, const PriorityDelegate &pd2) { return pd1.first < pd2.first; });
    }

private:
    using PriorityDelegate = std::pair<uint32_t, EventDelegate<EventT>>;
    using DelegateList = std::vector<PriorityDelegate>;
    using Queue = std::queue<EventT>;
    DelegateList delegates_;
    Queue queue_;
};

// This helper struct helps deduce the event type from a handler signature.
// Thanks to it, only the handler function pointer is required as a template parameter
// when calling the EventBus' subscribe methods.
template <typename S>
struct Signature;

template <typename R, typename Arg>
struct Signature<R (*)(Arg)>
{
    using return_type = R;
    using argument_type = Arg;
    using argument_type_decay = typename std::decay<Arg>::type;
};

template <typename C, typename R, typename Arg>
struct Signature<R (C::*)(Arg)>
{
    using return_type = R;
    using argument_type = Arg;
    using argument_type_decay = typename std::decay<Arg>::type;
};

template <typename C, typename R, typename Arg>
struct Signature<R (C::*)(Arg) const>
{
    using return_type = R;
    using argument_type = Arg;
    using argument_type_decay = typename std::decay<Arg>::type;
};

} // namespace detail

using EventID = hash_t;

/**
 * @brief Central message broker
 *
 */
class EventBus
{
public:
    /**
     * @brief Register a free function as an event handler.
     *
     * @tparam Function Function pointer as a handler. This is the only required template parameter,
     * the event type is deduced. The function must return true to consume an event, or false to let
     * it propagate to other handlers.
     * @tparam EventT Type of the event. This parameter is deduced from the handler's signature, there is
     * no need to fill it in.
     * @param priority Handlers with higher priority will execute first. In case two handlers have the same
     * priority, the last one to register will execute first.
     */
    template <auto Function, typename EventT = typename detail::Signature<decltype(Function)>::argument_type_decay,
              typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(Function), const EventT &>>>
    void subscribe(uint32_t priority = 0u)
    {
        auto *q_base_ptr = get_or_create<EventT>().get();
        auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
        q_ptr->template subscribe<Function>(priority);
    }

    /**
     * @brief Register a non-const member function as an event handler.
     *
     * @tparam Function Member function pointer as a handler. This is the only required template parameter,
     * the event type and class are deduced. The function must return true to consume an event, or false to let
     * it propagate to other handlers.
     * @tparam Class Class that holds the member function. This parameter is deduced from the instance pointer.
     * @tparam EventT Type of the event. This parameter is deduced from the handler's signature.
     * @param instance Pointer to the instance of the class holding the member function used as a handler.
     * @param priority Handlers with higher priority will execute first. In case two handlers have the same
     * priority, the last one to register will execute first.
     */
    template <
        auto MemberFunction, typename Class,
        typename EventT = typename detail::Signature<decltype(MemberFunction)>::argument_type_decay,
        typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(MemberFunction), Class *, const EventT &>>>
    void subscribe(Class &instance, uint32_t priority = 0u)
    {
        auto *q_base_ptr = get_or_create<EventT>().get();
        auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
        q_ptr->template subscribe<Class, MemberFunction>(&instance, priority);
    }

    /**
     * @brief Register a const member function as an event handler.
     *
     * @tparam Function Const member function pointer as a handler. This is the only required template parameter,
     * the event type and class are deduced. The function must return true to consume an event, or false to let
     * it propagate to other handlers.
     * @tparam Class Class that holds the member function. This parameter is deduced from the instance pointer.
     * @tparam EventT Type of the event. This parameter is deduced from the handler's signature.
     * @param instance Pointer to the instance of the class holding the member function used as a handler.
     * @param priority Handlers with higher priority will execute first. In case two handlers have the same
     * priority, the last one to register will execute first.
     */
    template <auto MemberFunction, typename Class,
              typename EventT = typename detail::Signature<decltype(MemberFunction)>::argument_type_decay,
              typename = std::enable_if_t<
                  std::is_invocable_r_v<bool, decltype(MemberFunction), const Class *, const EventT &>>>
    void subscribe(const Class &instance, uint32_t priority = 0u)
    {
        auto *q_base_ptr = get_or_create<EventT>().get();
        auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
        q_ptr->template subscribe<Class, MemberFunction>(&instance, priority);
    }

    /**
     * @brief Remove a previously registered free function event handler.
     *
     * @tparam Function The function to remove
     * @return true  if the removal succeeded
     * @return false otherwise
     */
    template <auto Function, typename EventT = typename detail::Signature<decltype(Function)>::argument_type_decay,
              typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(Function), const EventT &>>>
    bool unsubscribe()
    {
        auto *q_base_ptr = get_or_create<EventT>().get();
        auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
        return q_ptr->template unsubscribe<Function>();
    }

    /**
     * @brief Remove a previously registered non-const member function event handler.
     *
     * @tparam Function The function to remove
     * @return true  if the removal succeeded
     * @return false otherwise
     */
    template <
        auto MemberFunction, typename Class,
        typename EventT = typename detail::Signature<decltype(MemberFunction)>::argument_type_decay,
        typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(MemberFunction), Class *, const EventT &>>>
    bool unsubscribe(Class &instance)
    {
        auto *q_base_ptr = get_or_create<EventT>().get();
        auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
        return q_ptr->template unsubscribe<Class, MemberFunction>(&instance);
    }

    /**
     * @brief Remove a previously registered const member function event handler.
     *
     * @tparam Function The function to remove
     * @return true  if the removal succeeded
     * @return false otherwise
     */
    template <auto MemberFunction, typename Class,
              typename EventT = typename detail::Signature<decltype(MemberFunction)>::argument_type_decay,
              typename = std::enable_if_t<
                  std::is_invocable_r_v<bool, decltype(MemberFunction), const Class *, const EventT &>>>
    bool unsubscribe(const Class &instance)
    {
        auto *q_base_ptr = get_or_create<EventT>().get();
        auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
        return q_ptr->template unsubscribe<Class, MemberFunction>(&instance);
    }

    /**
     * @brief Fire an event and have it handled immediately
     * If there is no subscriber listening for this event type, this function does nothing.
     *
     * @tparam EventT The type of event to fire
     * @param event
     */
    template <typename EventT>
    void fire(const EventT &event)
    {
        try_get<EventT>([&event, this](auto *q_ptr) {
            (void)this;
#ifdef K_DEBUG
            track_event(event, false);
#endif
            q_ptr->fire(event);
        });
    }

    /**
     * @brief Enqueue an event for deferred handling, during the dispatch() call.
     * If there is no subscriber listening for this event type, this function does nothing.
     *
     * @tparam EventT The type of event to enqueue
     * @param event
     */
    template <typename EventT>
    void enqueue(const EventT &event)
    {
        try_get<EventT>([&event, this](auto *q_ptr) {
            (void)this;
#ifdef K_DEBUG
            track_event(event, true);
#endif
            q_ptr->submit(event);
        });
    }

    /**
     * @brief Enqueue an event for deferred handling, during the dispatch() call
     * If there is no subscriber listening for this event type, this function does nothing.
     *
     * @tparam EventT The type of event to enqueue
     * @param event An event rvalue to forward
     */
    template <typename EventT>
    void enqueue(EventT &&event)
    {
        try_get<EventT>([&event, this](auto *q_ptr) {
            (void)this;
#ifdef K_DEBUG
            track_event(event, true);
#endif
            q_ptr->submit(std::forward<EventT>(event));
        });
    }

    /**
     * @brief Handle all queued events. A timeout can be set so that event dispatching
     * will be interrupted after a certain amount of time, regardless of the unprocessed count.
     *
     * @param timeout Timeout duration. Leave / set to 0 to disable the timeout.
     * @return true if all events have been processed
     * @return false if the function timed out and there are consequently still some events in the queues.
     */
    bool dispatch(std::chrono::nanoseconds timeout = std::chrono::nanoseconds(0));

    /**
     * @brief Drop all events of the same type
     *
     * @tparam EventT The type of events to drop
     */
    template <typename EventT>
    void drop()
    {
        auto findit = event_queues_.find(kb::ctti::type_id<EventT>());
        if (findit != event_queues_.end())
            findit->second->drop();
    }

    /**
     * @brief Drop all enqueued events
     *
     */
    void drop();

    /**
     * @brief Check if all queues are empty
     *
     * @return true if all queues are empty
     * @return false otherwise
     */
    bool empty();

    /**
     * @brief Get the number of unprocessed events
     *
     * @return size_t
     */
    size_t get_unprocessed_count();

#ifdef K_DEBUG
    /**
     * @brief Setup a callback that decides whether a particular event type should be tracked or not.
     * Tracked events will be logged when enqueued or fired. An event that defines a stream operator
     * will automatically be serialized to the log stream.
     * The logger must be up and running for this to work. All events are logged on
     * the "event" channel. This channel must be created and configured beforehand.
     * This callback should be a fast lookup, it will be called each time an event is emitted.
     * Default implementation returns false.
     *
     * @param pred Any function that returns true if the argument ID corresponds to an event that
     * should be tracked, and false otherwise.
     */
    inline void set_event_tracking_predicate(std::function<bool(EventID)> pred)
    {
        should_track_ = pred;
    }
#endif

private:
#ifdef K_DEBUG
    // Log an event
    template <typename EventT>
    inline void track_event(const EventT &event, bool is_queued)
    {
        if (should_track_(kb::ctti::type_id<EventT>()))
        {
            constexpr std::string_view style_queued = "\033[1;38;2;0;0;0m\033[1;48;2;0;185;153m[q][";
            constexpr std::string_view style_fired = "\033[1;38;2;0;0;0m\033[1;48;2;185;153;0m[f][";
            // Using a concept we can know at compile-time if the event supports the stream operator
            if constexpr (detail::Streamable<EventT>)
            {
                kb::klog::get_log("event"_h, kb::klog::MsgType::EVENT, 0)
                    << (is_queued ? style_queued : style_fired) << kb::ctti::type_name<EventT>() << "]\033[0m " << event
                    << std::endl;
            }
            else
            {
                kb::klog::get_log("event"_h, kb::klog::MsgType::EVENT, 0)
                    << (is_queued ? style_queued : style_fired) << kb::ctti::type_name<EventT>() << "]\033[0m"
                    << std::endl;
            }
        }
    }
#endif

    // Helper function to get a particular event queue if it exists or create a new one if not
    template <typename EventT>
    auto &get_or_create()
    {
        auto &queue = event_queues_[kb::ctti::type_id<EventT>()];
        if (queue == nullptr)
            queue = std::make_unique<detail::EventQueue<EventT>>();

        return queue;
    }

    // Helper function to access a queue only if it exists
    template <typename EventT>
    void try_get(std::function<void(detail::EventQueue<EventT> *)> visit)
    {
        auto findit = event_queues_.find(kb::ctti::type_id<EventT>());
        if (findit != event_queues_.end())
        {
            auto *q_base_ptr = findit->second.get();
            auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
            visit(q_ptr);
        }
    }

private:
    using EventQueues = std::map<EventID, std::unique_ptr<detail::AbstractEventQueue>>;
    EventQueues event_queues_;

#ifdef K_DEBUG
    std::function<bool(EventID)> should_track_ = [](EventID) { return false; };
#endif
};

} // namespace event
} // namespace kb
