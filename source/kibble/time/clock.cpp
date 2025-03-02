#include "kibble/time/clock.h"

namespace kb
{

TimeBase::TimePoint TimeBase::s_start_time(std::chrono::steady_clock::now());

} // namespace kb