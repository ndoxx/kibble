#pragma once

#include "internal.h"

namespace backward
{
class StackTrace;
}

namespace kb
{

class StackTrace
{
public:
    StackTrace(size_t skip);
    StackTrace(const StackTrace&);
    StackTrace& operator=(const StackTrace&);

    std::string format() const;

private:
    friend struct internal_deleter::Deleter<backward::StackTrace>;
    internal_ptr<backward::StackTrace> ptrace_ = nullptr;
    size_t skip_ = 0;
};

} // namespace kb