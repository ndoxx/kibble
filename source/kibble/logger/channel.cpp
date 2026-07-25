#include "kibble/logger/channel.h"
#include "kibble/logger/entry.h"
#include "kibble/util/unordered_dense.h"

#include "atomic_queue/atomic_queue.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <thread>

namespace kb::log
{

bool Channel::s_exit_on_fatal_error_ = true;
bool Channel::s_intercept_signals_ = false;

namespace
{

constexpr unsigned k_queue_capacity = 8192;
constexpr auto k_idle_sleep = std::chrono::microseconds(200);

std::function<void(int)> g_panic_handler;

void os_signal_handler(int signal)
{
    g_panic_handler(signal);
}

class ThreadRegistry
{
public:
    static uint32_t id()
    {
        static thread_local uint32_t tid = register_this_thread();
        return tid;
    }

private:
    static uint32_t register_this_thread()
    {
        size_t key = std::hash<std::thread::id>{}(std::this_thread::get_id());

        std::lock_guard<std::mutex> lock(mutex_);
        auto [it, inserted] = ids_.try_emplace(key, next_id_);
        if (inserted)
        {
            ++next_id_;
        }
        return it->second;
    }

    static inline std::mutex mutex_;
    static inline ankerl::unordered_dense::map<size_t, uint32_t> ids_;
    static inline uint32_t next_id_ = 0;
};

struct QueuedEntry
{
    LogEntry entry;
    const Channel* channel = nullptr;
};

class LogWorker
{
public:
    static LogWorker& instance()
    {
        static LogWorker* worker = new LogWorker();
        return *worker;
    }

    static void shutdown()
    {
        LogWorker* worker = &instance();
        worker->wait_drained();
        // Cooperative, jthread dtor also joins
        worker->thread_.request_stop();
        delete worker;
    }

    void push(QueuedEntry&& qe)
    {
        queue_.push(std::move(qe));
        pushed_.fetch_add(1, std::memory_order_relaxed);
    }

    void wait_drained() const
    {
        uint64_t target = pushed_.load(std::memory_order_relaxed);
        while (processed_.load(std::memory_order_acquire) < target)
        {
            std::this_thread::yield();
        }
    }

    void panic()
    {
        // Same mechanism as normal shutdown
        thread_.request_stop();
    }

    LogWorker(const LogWorker&) = delete;
    LogWorker& operator=(const LogWorker&) = delete;

private:
    LogWorker() : queue_(k_queue_capacity), thread_(std::bind_front(&LogWorker::run, this))
    {
    }

    void run(std::stop_token stoken)
    {
        QueuedEntry qe;
        while (true)
        {
            if (queue_.try_pop(qe))
            {
                detail::dispatch_entry(*qe.channel, qe.entry);
                processed_.fetch_add(1, std::memory_order_release);
                continue;
            }

            if (stoken.stop_requested())
            {
                break;
            }

            std::this_thread::sleep_for(k_idle_sleep);
        }
    }

    atomic_queue::AtomicQueueB2<QueuedEntry> queue_;
    std::atomic<uint64_t> pushed_{0};
    std::atomic<uint64_t> processed_{0};
    std::jthread thread_;
};

} // namespace

namespace detail
{
void dispatch_entry(const Channel& channel, const LogEntry& entry)
{
    std::lock_guard<std::mutex> lock(channel.sink_mutex_);
    for (auto& psink : channel.sinks_)
    {
        psink->submit(entry, channel.presentation_);
    }
}
} // namespace detail

Channel::Channel(Severity level, const std::string& full_name, const std::string& short_name, math::argb32_t tag_color)
    : presentation_{full_name, short_name, tag_color}, level_(level)
{
}

Channel::~Channel()
{
    LogWorker::instance().wait_drained();
}

void Channel::shutdown()
{
    LogWorker::shutdown();
}

void Channel::attach_sink(std::shared_ptr<Sink> psink)
{
    std::lock_guard<std::mutex> lock(sink_mutex_);
    sinks_.push_back(std::move(psink));
    sinks_.back()->on_attach(*this);
}

void Channel::detach_sink(const std::shared_ptr<Sink>& psink)
{
    std::lock_guard<std::mutex> lock(sink_mutex_);
    sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), psink), sinks_.end());
}

void Channel::attach_policy(std::shared_ptr<Policy> ppolicy)
{
    policies_.push_back(std::move(ppolicy));
}

void Channel::submit(LogEntry&& entry) const
{
    if (entry.severity > level_)
    {
        return;
    }

    entry.thread_id = ThreadRegistry::id();

    bool fatal = entry.severity == Severity::Fatal;

    for (const auto& ppol : policies_)
    {
        if (!ppol->transform_filter(entry))
        {
            return;
        }
    }

    LogWorker::instance().push({std::move(entry), this});

    if (s_exit_on_fatal_error_ && fatal)
    {
        LogWorker::instance().wait_drained();

        for (auto& psink : sinks_)
        {
            psink->flush();
        }

        exit(1);
    }
}

void Channel::flush() const
{
    LogWorker::instance().wait_drained();

    for (const auto& psink : sinks_)
    {
        psink->flush();
    }
}

void Channel::intercept_signals(bool value)
{
    s_intercept_signals_ = value;

    static bool s_signal_handler_configured = false;
    if (s_intercept_signals_ && !s_signal_handler_configured)
    {
        (void)std::signal(SIGABRT, os_signal_handler);
        (void)std::signal(SIGFPE, os_signal_handler);
        (void)std::signal(SIGILL, os_signal_handler);
        (void)std::signal(SIGINT, os_signal_handler);
        (void)std::signal(SIGSEGV, os_signal_handler);
        (void)std::signal(SIGTERM, os_signal_handler);

        g_panic_handler = [](int) { LogWorker::instance().panic(); };
        s_signal_handler_configured = true;
    }
}

} // namespace kb::log