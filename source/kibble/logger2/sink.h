#pragma once

namespace kb::log
{

class Sink
{
public:
    virtual ~Sink() = default;
    virtual void submit(const struct Entry &) = 0;
    virtual void on_attach() {};
    virtual void on_detach() {};
};

} // namespace kb::log