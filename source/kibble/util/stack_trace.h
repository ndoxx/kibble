#pragma once

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
    StackTrace(size_t skip);
    ~StackTrace();
    StackTrace(const StackTrace&);
    StackTrace& operator=(const StackTrace&);

    std::string format() const;

private:
    std::unique_ptr<backward::StackTrace> ptrace_;
    size_t skip_ = 0;
};

} // namespace kb