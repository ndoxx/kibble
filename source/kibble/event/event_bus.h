/*
 * This is a rewrite of the event system used in my projects WCore and ErwinEngine
 * Inspired by https://medium.com/@savas/nomad-game-engine-part-7-the-event-system-45a809ccb68f
 * Differences are:
 * - Using const references or rvalues instead of pointers to pass events around
 * - Using my constexpr ctti::type_id<>() instead of RTTI
 * - Leak free
 * - Events can be any type, no need to derive from a base event class
 * - Deferred event handling with event queues, instant firing still supported
 * - Priority mechanism
 *
 * TODO:
 * [ ] Dispatch timeout
 * [ ] Dispatch deadlock detection?
 * [ ] Event pooling if deemed necessary
 */

#pragma once
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <queue>
#include <string>
#include <type_traits>
#include <vector>

#include "../config/config.h"
#include "../ctti/ctti.h"
#include "../logger/logger.h"

namespace kb
{
namespace event
{

namespace detail
{
// Interface for function wrappers that an event bus can register
template <typename EventT> class AbstractDelegate
{
public:
    virtual ~AbstractDelegate() = default;
    inline bool exec(const EventT &event) const
    {
        return call(event);
    }

private:
    virtual bool call(const EventT &event) const = 0;
};

// Member function wrapper, to allow classes to register their member functions as event handlers
template <typename T, typename EventT> class MemberDelegate : public AbstractDelegate<EventT>
{
public:
    typedef bool (T::*MemberFunction)(const EventT &);

    MemberDelegate(T *instance, MemberFunction memberFunction) noexcept
        : instance{instance}, memberFunction{memberFunction} {};

    // Cast event to the correct type and call member function
    bool call(const EventT &event) const override final
    {
        return (instance->*memberFunction)(event);
    }

private:
    T *instance;                   // Pointer to class instance
    MemberFunction memberFunction; // Pointer to member function
};

// Free function wrapper
template <typename EventT> class FreeDelegate : public AbstractDelegate<EventT>
{
public:
    typedef bool (*FreeFunction)(const EventT &);

    explicit FreeDelegate(FreeFunction freeFunction) noexcept : freeFunction{freeFunction} {};

    // Cast event to the correct type and call member function
    bool call(const EventT &event) const override final
    {
        return (*freeFunction)(event);
    }

private:
    FreeFunction freeFunction; // Pointer to member function
};

// Interface for an event queue
class AbstractEventQueue
{
public:
    virtual ~AbstractEventQueue() = default;
    virtual void process() = 0;
    virtual bool empty() const = 0;
    virtual size_t size() const = 0;
};

// Concrete event queue, can subscribe functions, and process events immediately or in a deferred fashion
template <typename EventT> class EventQueue : public AbstractEventQueue
{
public:
    template <typename ClassT>
    inline void subscribe(ClassT *instance, bool (ClassT::*memberFunction)(const EventT &), uint32_t priority)
    {
        delegates_.push_back({priority, std::make_unique<MemberDelegate<ClassT, EventT>>(instance, memberFunction)});
        // greater_equal generates the desired behavior: for equal priority range of subscribers,
        // latest subscriber handles the event first
        std::sort(delegates_.begin(), delegates_.end(), std::greater_equal<PriorityDelegate>());
    }

    inline void subscribe(bool (*freeFunction)(const EventT &), uint32_t priority)
    {
        delegates_.push_back({priority, std::make_unique<FreeDelegate<EventT>>(freeFunction)});
        std::sort(delegates_.begin(), delegates_.end(), std::greater_equal<PriorityDelegate>());
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
            if (handler->exec(event))
                break; // If handler returns true, event is not propagated further
    }

    void process() override final
    {
        while (!queue_.empty())
        {
            for (auto &&[priority, handler] : delegates_)
                if (handler->exec(queue_.front()))
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
    using PriorityDelegate = std::pair<uint32_t, std::unique_ptr<AbstractDelegate<EventT>>>;
    using DelegateList = std::vector<PriorityDelegate>;
    using Queue = std::queue<EventT>;
    DelegateList delegates_;
    Queue queue_;
};

template <typename T> concept Streamable = requires(std::ostream &stream, T a)
{
    stream << a;
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
     * @brief Subscribe a free function to a particular event type
     *
     * @tparam EventT The event type that will trigger the function execution
     * @param freeFunction A free function pointer to execute when the event is dispatched
     * @param priority Subscribers with a greater priority will execute first. If two subscribers
     * have the same priority, then the most recent subscriber will execute first.
     */
    template <typename EventT> void subscribe(bool (*freeFunction)(const EventT &), uint32_t priority = 0u)
    {
        auto *q_base_ptr = get_or_create<EventT>().get();
        auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
        q_ptr->subscribe(freeFunction, priority);
    }

    /**
     * @brief Subscribe a member function to a particular event type
     *
     * @tparam ClassT The class owning the specified member
     * @tparam EventT The event type that will trigger the function execution
     * @param instance Instance of the class that subscribes to the event
     * @param memberFunction Member function pointer to execute when the event is dispatched
     * @param priority Subscribers with a greater priority will execute first. If two subscribers
     * have the same priority, then the most recent subscriber will execute first.
     */
    template <typename ClassT, typename EventT>
    void subscribe(ClassT *instance, bool (ClassT::*memberFunction)(const EventT &), uint32_t priority = 0u)
    {
        auto *q_base_ptr = get_or_create<EventT>().get();
        auto *q_ptr = static_cast<detail::EventQueue<EventT> *>(q_base_ptr);
        q_ptr->subscribe(instance, memberFunction, priority);
    }

    /**
     * @brief Fire an event and have it handled immediately
     *
     * @tparam EventT The type of event to fire
     * @param event
     */
    template <typename EventT> void fire(const EventT &event)
    {
        try_get<EventT>([&event, this](auto *q_ptr) {
            (void)this;
#ifdef K_DEBUG
            track_event(event);
#endif
            q_ptr->fire(event);
        });
    }

    /**
     * @brief Enqueue an event for deferred handling, during the dispatch() call
     *
     * @tparam EventT The type of event to enqueue
     * @param event
     */
    template <typename EventT> void enqueue(const EventT &event)
    {
        try_get<EventT>([&event, this](auto *q_ptr) {
            (void)this;
#ifdef K_DEBUG
            track_event(event);
#endif
            q_ptr->submit(event);
        });
    }

    /**
     * @brief Enqueue an event for deferred handling, during the dispatch() call
     *
     * @tparam EventT The type of event to enqueue
     * @param event An event rvalue to forward
     */
    template <typename EventT> void enqueue(EventT &&event)
    {
        try_get<EventT>([&event, this](auto *q_ptr) {
            (void)this;
#ifdef K_DEBUG
            track_event(event);
#endif
            q_ptr->submit(std::forward<EventT>(event));
        });
    }

    /**
     * @brief Handle all queued events
     *
     */
    void dispatch();

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
     * Tracked events will be logged when enqueued or fired. The logger must
     * be up and running for this to work. All events are logged on
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
    template <typename EventT> inline void track_event(const EventT &event)
    {
        if (should_track_(kb::ctti::type_id<EventT>()))
        {
            // Using a concept we can know at compile-time if the event supports the stream operator
            if constexpr (detail::Streamable<EventT>)
            {
                kb::klog::get_log("event"_h, kb::klog::MsgType::EVENT, 0)
                    << "\033[1;38;2;0;0;0m\033[1;48;2;0;185;153m[" << kb::ctti::type_name<EventT>() << "]\033[0m "
                    << event << std::endl;
            }
            else
            {
                kb::klog::get_log("event"_h, kb::klog::MsgType::EVENT, 0)
                    << "\033[1;38;2;0;0;0m\033[1;48;2;0;185;153m[" << kb::ctti::type_name<EventT>() << "]\033[0m"
                    << std::endl;
            }
        }
    }
#endif

    // Helper function to get a particular event queue if it exists or create a new one if not
    template <typename EventT> auto &get_or_create()
    {
        auto &queue = event_queues_[kb::ctti::type_id<EventT>()];
        if (queue == nullptr)
            queue = std::make_unique<detail::EventQueue<EventT>>();

        return queue;
    }

    // Helper function to access a queue only if it exists
    template <typename EventT> void try_get(std::function<void(detail::EventQueue<EventT> *)> visit)
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
