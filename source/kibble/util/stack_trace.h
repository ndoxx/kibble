#pragma once

#include "internal.h"
#include <memory>

namespace backward
{
class StackTrace;
}

namespace kb
{

class StackTrace
{
public:
    StackTrace();
    StackTrace(const StackTrace &);
    StackTrace &operator=(const StackTrace &);

    std::string format() const;

private:
    friend struct internal_deleter::Deleter<backward::StackTrace>;
    internal_ptr<backward::StackTrace> ptrace_ = nullptr;
};

} // namespace kb