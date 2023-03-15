#pragma once
#include "policy.h"
#include "severity.h"
#include "sink.h"
#include "../math/color.h"
#include <memory>
#include <vector>

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
    Channel(Severity level, const std::string& full_name, const std::string& short_name, math::argb32_t tag_color);

    void attach_sink(std::shared_ptr<Sink> psink);
    void attach_policy(std::shared_ptr<Policy> ppolicy);

    inline void set_severity_level(Severity level)
    {
        level_ = level;
    }

    void submit(struct LogEntry &entry);

private:
    ChannelPresentation presentation_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    std::vector<std::shared_ptr<Policy>> policies_;

    Severity level_;
};

} // namespace kb::log