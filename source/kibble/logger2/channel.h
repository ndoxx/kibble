#pragma once
#include "../math/color.h"
#include "policy.h"
#include "severity.h"
#include "sink.h"
#include <memory>
#include <vector>

namespace kb::th
{
class JobSystem;
}

namespace kb::log
{

struct ChannelPresentation
{
    std::string full_name;
    std::string tag;
};

class Channel
{
public:
    Channel(Severity level, const std::string &full_name, const std::string &short_name, math::argb32_t tag_color);

    void attach_sink(std::shared_ptr<Sink> psink);
    void attach_policy(std::shared_ptr<Policy> ppolicy);

    inline void set_severity_level(Severity level)
    {
        level_ = level;
    }

    static inline void set_async(th::JobSystem *js, uint32_t worker = 1)
    {
        s_js_ = js;
        s_worker_ = worker;
    }

    static inline void exit_on_fatal_error(bool value = true)
    {
        s_exit_on_fatal_error_ = value;
    }

    void submit(struct LogEntry &&entry) const;

private:
    ChannelPresentation presentation_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    std::vector<std::shared_ptr<Policy>> policies_;
    Severity level_;

    static th::JobSystem *s_js_;
    static uint32_t s_worker_;
    static bool s_exit_on_fatal_error_;
};

} // namespace kb::log