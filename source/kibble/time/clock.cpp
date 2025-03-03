#include "kibble/time/clock.h"

namespace kb
{

TimeBase::TimePoint TimeBase::s_start_time(StdClock::now());

} // namespace kb