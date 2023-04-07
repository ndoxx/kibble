#include "channel.h"
#include "entry.h"
#include "thread/job/job_system.h"
#include <csignal>
#include <fmt/color.h>
#include <fmt/core.h>

namespace kb::log
{

th::JobSystem* Channel::s_js_ = nullptr;
uint32_t Channel::s_worker_ = 1;
bool Channel::s_exit_on_fatal_error_ = true;
bool Channel::s_intercept_signals_ = false;

namespace
{
std::function<void(int)> g_panic_handler;
// Wrapper for the functional signal handler
void panic_handler(int signal)
{
    g_panic_handler(signal);
}
} // namespace

Channel::Channel(Severity level, const std::string& full_name, const std::string& short_name, math::argb32_t tag_color)
    : presentation_{full_name, short_name, tag_color}, level_(level)
{
}

void Channel::attach_sink(std::shared_ptr<Sink> psink)
{
    sinks_.push_back(psink);
    sinks_.back()->on_attach(*this);
}

void Channel::attach_policy(std::shared_ptr<Policy> ppolicy)
{
    policies_.push_back(ppolicy);
}

void Channel::submit(LogEntry&& entry) const
{
    // Check if the severity level is high enough
    if (entry.severity > level_)
        return;

    bool fatal = entry.severity == Severity::Fatal;

    // Check compliance with policies
    for (const auto& ppol : policies_)
        if (!ppol->transform_filter(entry))
            return;

    // Send to all attached sinks
    if (s_js_ == nullptr)
    {
        for (auto& psink : sinks_)
            psink->submit_lock(entry, presentation_);
    }
    else
    {
        // Set thread id
        entry.thread_id = s_js_->this_thread_id();
        th::JobMetadata meta(th::force_worker(s_worker_), "Log");
        meta.essential__ = true;
        // Schedule logging task. Log entry is moved.
        auto&& [task, future] = s_js_->create_task(meta, [this, entry = std::move(entry)]() {
            for (auto& psink : sinks_)
                psink->submit(entry, presentation_);
        });
        task.schedule();
    }

    if (s_exit_on_fatal_error_ && fatal)
    {
        if (s_js_)
            s_js_->shutdown();

        for (auto& psink : sinks_)
            psink->flush();

        exit(0);
    }
}

void Channel::flush() const
{
    for (const auto& psink : sinks_)
        psink->flush();
}

void Channel::set_async(th::JobSystem* js, uint32_t worker)
{
    s_js_ = js;
    s_worker_ = worker;

    if (s_intercept_signals_)
    {
        // Intercept all termination signals
        // Force the job system into panic mode when a signal is intercepted

        static bool s_signal_handler_configured = false;
        if (s_js_ && !s_signal_handler_configured)
        {
            std::signal(SIGABRT, panic_handler);
            std::signal(SIGFPE, panic_handler);
            std::signal(SIGILL, panic_handler);
            std::signal(SIGINT, panic_handler);
            std::signal(SIGSEGV, panic_handler);
            std::signal(SIGTERM, panic_handler);

            g_panic_handler = [](int) { s_js_->abort(); };
            s_signal_handler_configured = true;
        }
    }
}

} // namespace kb::log