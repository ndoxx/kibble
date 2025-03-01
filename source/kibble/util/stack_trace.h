#pragma once

#include <memory>
#include <string>

namespace cpptrace
{
struct raw_trace;
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

    std::string format(bool color = true) const;

private:
    std::unique_ptr<cpptrace::raw_trace> ptrace_;
    size_t skip_ = 0;
};

} // namespace kb