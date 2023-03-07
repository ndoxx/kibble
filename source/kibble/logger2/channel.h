#pragma once
#include <memory>
#include <vector>
#include "sink.h"
#include "policy.h"

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
    Channel(const ChannelPresentation& presentation);

    void attach_sink(std::shared_ptr<Sink> psink);
    void attach_policy(std::shared_ptr<Policy> ppolicy);

    void submit(const struct LogEntry& entry);

private:
    ChannelPresentation presentation_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    std::vector<std::shared_ptr<Policy>> policies_;
};

}