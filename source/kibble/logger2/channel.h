#pragma once
#include <memory>
#include <vector>
#include "sink.h"
#include "policy.h"

namespace kb::log
{

struct ChannelPresentation
{
    std::string full_name_;
    std::string short_name_;
};

class Channel
{
public:

private:
    ChannelPresentation presentation_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    std::vector<std::shared_ptr<Policy>> policies_;
};

}