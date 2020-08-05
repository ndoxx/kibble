#include "time_base.h"

namespace kb
{

TimePoint TimeBase::START_TIME(std::chrono::high_resolution_clock::now());

} // namespace kb