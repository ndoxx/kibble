/**
 * @file event_bus.h
 * @brief This is a rewrite of the event system used in my projects WCore and ErwinEngine.
 *
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
 *
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021
 *
 */
#pragma once

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <queue>
#include <type_traits>
#include <vector>

#include "kibble/ctti/ctti.h"
#include "kibble/event/event_observer.h"
#include "kibble/time/clock.h"
#include "kibble/util/delegate.h"

namespace kb::event
{

namespace detail
{

template <typename T>
concept DerivedFromEventObserver = std::is_base_of_v<EventObserver, T>;

template <typename EventT>
using EventDelegate = kb::Delegate<bool(const EventT&)>;

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds>;

template <typename T>
using to_pointer = std::add_pointer<typename std::remove_reference<T>::type>;

/**
 * @internal
 * @brief Interface for an event queue.
 *
 */
class AbstractEventQueue
{
public:
    virtual ~AbstractEventQueue() = default;

    virtual bool process(TimePoint, EventObserver* observer) = 0;
    virtual void drop() = 0;
    virtual bool empty() const = 0;
    virtual size_t size() const = 0;
};

/**
 * @internal
 * @brief Concrete event queue, can subscribe functions, and process events immediately or in a deferred fashion.
 *
 * @tparam EventT
 */
template <typename EventT>
class EventQueue : public kb::event::detail::AbstractEventQueue
{
public:
    /**
     * @internal
     * @brief Subscribe a free function.
     *
     * @tparam Function
     * @param priority
     */
    template <std::invocable<const EventT&> auto Function>
    inline void subscribe(uint32_t priority)
    {
        auto delegate = EventDelegate<EventT>::template create<Function>();
        if (std::none_of(delegates_.begin(), delegates_.end(),
                         [&delegate](const auto& p) { return p.second == delegate; }))
        {
            delegates_.emplace_back(priority, delegate);
            needs_sort_ = true;
        }
    }

    /**
     * @internal
     * @brief Subscribe a member function.
     *
     * I use template magic to handle both const and non-const cases at the same time.
     *
     * @tparam ClassRef either a reference or a const reference type
     * @tparam MemberFunction
     * @param instance class instance
     * @param priority
     */
    template <typename ClassRef, auto MemberFunction>
    inline void subscribe(typename to_pointer<ClassRef>::type instance, uint32_t priority)
    {
        auto delegate = EventDelegate<EventT>::template create<MemberFunction>(instance);
        if (std::none_of(delegates_.begin(), delegates_.end(),
                         [&delegate](const auto& p) { return p.second == delegate; }))
        {
            delegates_.emplace_back(priority, delegate);
            needs_sort_ = true;
        }
    }

    /**
     * @internal
     * @brief Unsubscribe a free function.
     *
     * @tparam Function
     * @return true if the delegate was found and erased
     * @return false otherwise
     */
    template <std::invocable<const EventT&> auto Function>
    bool unsubscribe()
    {
        auto target = EventDelegate<EventT>::template create<Function>();
        auto initial_size = delegates_.size();
        std::erase_if(delegates_, [&target](const auto& p) { return p.second == target; });
        return delegates_.size() < initial_size;
    }

    /**
     * @internal
     * @brief Unsubscribe a member function.
     *
     * I use template magic to handle both const and non-const cases at the same time.
     *
     * @tparam ClassRef either a reference or a const reference type
     * @tparam MemberFunction
     * @param instance class instance
     * @return true if the delegate was found and erased
     * @return false otherwise
     */
    template <typename ClassRef, auto MemberFunction>
    bool unsubscribe(typename to_pointer<ClassRef>::type instance)
    {
        auto target = EventDelegate<EventT>::template create<MemberFunction>(instance);
        auto initial_size = delegates_.size();
        std::erase_if(delegates_, [&target](const auto& p) { return p.second == target; });
        return delegates_.size() < initial_size;
    }

    // clang-format off
    /// @internal @brief Drop all events of this queue.
    void drop() override final               { Queue{}.swap(queue_); /* Swap with an empty queue */ }
    
    /// @internal @brief Check if the queue is empty.
    bool empty() const override final        { return queue_.empty(); }

    /// @internal @brief Get the number of events in this queue.
    size_t size() const override final       { return queue_.size(); }

    /// @internal @brief Submit an event to this queue.
    inline void enqueue(const EventT& event) { queue_.push(event); }

    /// @internal @brief Submit an event to this queue (r-value version).
    inline void enqueue(EventT&& event)      { queue_.push(std::move(event)); }
    // clang-format on

    /**
     * @internal
     * @brief Fire an event instantly.
     *
     * All the delegates are iterates and executed. The queue is left untouched.
     *
     * @param event
     */
    void fire(const EventT& event, EventObserver* observer)
    {
        ensure_sorted();
        dispatch(event, observer);
    }

    /**
     * @internal
     * @brief Process all events until a deadline is reached.
     *
     * While the queue is not empty, pop an event and execute all the delegates on it.
     *
     * @param deadline time point at which the event processing should be interrupted.
     * @return true if all the events have been processed
     * @return false if the function timed out, and there are still events in the queue
     */
    bool process(TimePoint deadline, EventObserver* observer) override final
    {
        ensure_sorted();

        while (!queue_.empty())
        {
            dispatch(queue_.front(), observer);
            queue_.pop();

            // Timeout if the deadline was exceeded
            if (nanoClock::now() > deadline)
            {
                return false;
            }
        }

        return true;
    }

private:
    /// @internal @brief Sort the delegate list according to delegate priority if needed.
    void ensure_sorted()
    {
        if (needs_sort_)
        {
            needs_sort_ = false;
            std::sort(delegates_.begin(), delegates_.end(),
                      [](const PriorityDelegate& pd1, const PriorityDelegate& pd2) { return pd1.first < pd2.first; });
        }
    }

    /// @internal @brief Iterate delegate list in reverse order and call handlers
    bool dispatch(const EventT& event, EventObserver* observer)
    {
        // Iterate backwards by index - safe even if elements are removed during iteration
        for (size_t ii = delegates_.size(); ii-- > 0;)
        {
            if (observer != nullptr)
            {
                observer->on_event(EventInfo{
                    &event,
                    kb::ctti::type_id<EventT>(),
                    kb::ctti::type_name<EventT>(),
                    EventInfo::Phase::Handle,
                });
            }

            // Check bounds in case delegate was removed
            if (ii < delegates_.size() && delegates_[ii].second(event))
            {
                // If handler returns true, event is not propagated further
                return true;
            }
        }
        return false;
    }

private:
    using PriorityDelegate = std::pair<uint32_t, EventDelegate<EventT>>;
    using DelegateList = std::vector<PriorityDelegate>;
    using Queue = std::queue<EventT>;
    DelegateList delegates_;
    Queue queue_;
    bool needs_sort_{false};
};

/**
 * @internal
 * @brief This helper struct helps deduce the event type from a handler signature.
 *
 * Thanks to it, only the handler function pointer is required as a template parameter when calling the EventBus'
 * subscribe methods.
 *
 * @tparam S
 */
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
    EventBus() = default;
    ~EventBus();

    /**
     * @brief Create and set an event observer.
     * The EventBus takes ownership of the observer.
     * If an observer already exists, it will be deleted first.
     *
     * @tparam T Observer type, must derive from EventObserver
     * @tparam Args Constructor argument types
     * @param args Arguments to forward to the observer's constructor
     * @return Pointer to the created observer
     */
    template <detail::DerivedFromEventObserver T, typename... Args>
    T* create_observer(Args&&... args)
    {
        // Delete existing observer if any
        delete observer_;

        // Create new observer
        auto* new_observer = new T(std::forward<Args>(args)...);
        observer_ = new_observer;

        return new_observer;
    }

    /**
     * @brief Remove and delete the current observer.
     */
    void remove_observer()
    {
        delete observer_;
        observer_ = nullptr;
    }

    /**
     * @brief Get the current observer (if any).
     *
     * @return Pointer to the observer, or nullptr if none exists
     */
    EventObserver* get_observer() const
    {
        return observer_;
    }

    /**
     * @brief Get the current observer cast to a specific type.
     *
     * @tparam T The type to cast to
     * @return Pointer to the observer cast to T, or nullptr if the cast fails or no observer exists
     */
    template <detail::DerivedFromEventObserver T>
    T* get_observer_as() const
    {
        return dynamic_cast<T*>(observer_);
    }

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
              typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(Function), const EventT&>>>
    void subscribe(uint32_t priority = 0u)
    {
        auto* q_base_ptr = get_or_create<EventT>().get();
        auto* q_ptr = static_cast<detail::EventQueue<EventT>*>(q_base_ptr);
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
    template <auto MemberFunction, typename Class,
              typename EventT = typename detail::Signature<decltype(MemberFunction)>::argument_type_decay,
              typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(MemberFunction), Class*, const EventT&>>>
    void subscribe(Class& instance, uint32_t priority = 0u)
    {
        auto* q_base_ptr = get_or_create<EventT>().get();
        auto* q_ptr = static_cast<detail::EventQueue<EventT>*>(q_base_ptr);
        q_ptr->template subscribe<Class&, MemberFunction>(&instance, priority);
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
    template <
        auto MemberFunction, typename Class,
        typename EventT = typename detail::Signature<decltype(MemberFunction)>::argument_type_decay,
        typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(MemberFunction), const Class*, const EventT&>>>
    void subscribe(const Class& instance, uint32_t priority = 0u)
    {
        auto* q_base_ptr = get_or_create<EventT>().get();
        auto* q_ptr = static_cast<detail::EventQueue<EventT>*>(q_base_ptr);
        q_ptr->template subscribe<const Class&, MemberFunction>(&instance, priority);
    }

    /**
     * @brief Remove a previously registered free function event handler.
     *
     * @tparam Function The function to remove
     * @return true  if the removal succeeded
     * @return false otherwise
     */
    template <auto Function, typename EventT = typename detail::Signature<decltype(Function)>::argument_type_decay,
              typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(Function), const EventT&>>>
    bool unsubscribe()
    {
        auto* q_base_ptr = get_or_create<EventT>().get();
        auto* q_ptr = static_cast<detail::EventQueue<EventT>*>(q_base_ptr);
        return q_ptr->template unsubscribe<Function>();
    }

    /**
     * @brief Remove a previously registered non-const member function event handler.
     *
     * @tparam Function The function to remove
     * @return true  if the removal succeeded
     * @return false otherwise
     */
    template <auto MemberFunction, typename Class,
              typename EventT = typename detail::Signature<decltype(MemberFunction)>::argument_type_decay,
              typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(MemberFunction), Class*, const EventT&>>>
    bool unsubscribe(Class& instance)
    {
        auto* q_base_ptr = get_or_create<EventT>().get();
        auto* q_ptr = static_cast<detail::EventQueue<EventT>*>(q_base_ptr);
        return q_ptr->template unsubscribe<Class&, MemberFunction>(&instance);
    }

    /**
     * @brief Remove a previously registered const member function event handler.
     *
     * @tparam Function The function to remove
     * @return true  if the removal succeeded
     * @return false otherwise
     */
    template <
        auto MemberFunction, typename Class,
        typename EventT = typename detail::Signature<decltype(MemberFunction)>::argument_type_decay,
        typename = std::enable_if_t<std::is_invocable_r_v<bool, decltype(MemberFunction), const Class*, const EventT&>>>
    bool unsubscribe(const Class& instance)
    {
        auto* q_base_ptr = get_or_create<EventT>().get();
        auto* q_ptr = static_cast<detail::EventQueue<EventT>*>(q_base_ptr);
        return q_ptr->template unsubscribe<const Class&, MemberFunction>(&instance);
    }

    /**
     * @brief Fire an event and have it handled immediately.
     * If there is no subscriber listening for this event type, this function does nothing.
     *
     * @tparam EventT The type of event to fire
     * @param event
     */
    template <typename EventT>
    void fire(const EventT& event)
    {
        auto* q_ptr = try_get<EventT>();
        if (!q_ptr)
        {
            return;
        }

        notify_observer(EventInfo{
            &event,
            kb::ctti::type_id<EventT>(),
            kb::ctti::type_name<EventT>(),
            EventInfo::Phase::Fire,
        });

        q_ptr->fire(event, observer_);
    }

    /**
     * @brief Enqueue an event for deferred handling, during the dispatch() call.
     * If there is no subscriber listening for this event type, this function does nothing.
     *
     * @tparam EventT The type of event to enqueue
     * @param event
     */
    template <typename EventT>
    void enqueue(const EventT& event)
    {
        auto* q_ptr = try_get<EventT>();
        if (!q_ptr)
        {
            return;
        }

        notify_observer(EventInfo{
            &event,
            kb::ctti::type_id<EventT>(),
            kb::ctti::type_name<EventT>(),
            EventInfo::Phase::Enqueue,
        });

        q_ptr->enqueue(event);
    }

    /**
     * @brief Enqueue an event for deferred handling, during the dispatch() call.
     * If there is no subscriber listening for this event type, this function does nothing.
     *
     * @tparam EventT The type of event to enqueue
     * @param event An event rvalue to forward
     */
    template <typename EventT>
    void enqueue(EventT&& event)
    {
        auto* q_ptr = try_get<EventT>();
        if (!q_ptr)
        {
            return;
        }

        notify_observer(EventInfo{
            &event,
            kb::ctti::type_id<EventT>(),
            kb::ctti::type_name<EventT>(),
            EventInfo::Phase::Enqueue,
        });

        q_ptr->enqueue(std::forward<EventT>(event));
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
     * @brief Drop all events of the same type.
     *
     * @tparam EventT The type of events to drop
     */
    template <typename EventT>
    void drop()
    {
        auto findit = event_queues_.find(kb::ctti::type_id<EventT>());
        if (findit != event_queues_.end())
        {
            findit->second->drop();
        }
    }

    /**
     * @brief Drop all enqueued events.
     *
     */
    void drop();

    /**
     * @brief Check if all queues are empty.
     *
     * @return true if all queues are empty
     * @return false otherwise
     */
    bool empty();

    /**
     * @brief Get the number of unprocessed events.
     *
     * @return size_t
     */
    size_t get_unprocessed_count();

    /**
     * @brief Check if an event queue already exists.
     *
     * @tparam EventT
     * @return true
     * @return false
     */
    template <typename EventT>
    inline bool has_queue()
    {
        return (event_queues_[kb::ctti::type_id<EventT>()] != nullptr);
    }

private:
    /**
     * @internal
     * @brief Helper function to get a particular event queue if it exists or create a new one if not.
     *
     * @tparam EventT event type
     * @return the event queue for this event type
     */
    template <typename EventT>
    auto& get_or_create()
    {
        auto& queue = event_queues_[kb::ctti::type_id<EventT>()];
        if (queue == nullptr)
        {
            queue = std::make_unique<detail::EventQueue<EventT>>();
        }

        return queue;
    }

    /**
     * @internal
     * @brief Get a typed event queue pointer if it exists, nullptr otherwise.
     *
     * @tparam EventT event type
     * @return Pointer to the queue, or nullptr if no subscribers exist for this event type
     */
    template <typename EventT>
    detail::EventQueue<EventT>* try_get()
    {
        auto findit = event_queues_.find(kb::ctti::type_id<EventT>());
        if (findit != event_queues_.end())
        {
            return static_cast<detail::EventQueue<EventT>*>(findit->second.get());
        }
        return nullptr;
    }

    /**
     * @internal
     * @brief Nortify the observer of event processing status
     *
     * @param info
     */
    void notify_observer(const EventInfo& info);

private:
    // NOTE(ndx): Not using std::unordered_map here because we need subscription order to have
    // a deterministic effect on event processing order.
    using EventQueues = std::map<EventID, std::unique_ptr<detail::AbstractEventQueue>>;
    EventQueues event_queues_;
    EventObserver* observer_{nullptr};
};

} // namespace kb::event
