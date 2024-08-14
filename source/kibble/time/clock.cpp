#include "kibble/time/clock.h"

namespace kb
{

TimeBase::HRCTimePoint TimeBase::s_start_time(std::chrono::high_resolution_clock::now());

} // namespace kb