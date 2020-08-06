#pragma once

#include <chrono>

namespace kb
{

template <typename DurationT> class Clock
{
public:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock, DurationT>;

    static inline TimePoint now()
    {
        return std::chrono::time_point_cast<DurationT>(std::chrono::high_resolution_clock::now());
    }

    Clock() : time_point_(now()) {}

    inline DurationT get_elapsed_time() const
    {
        return now() - time_point_;
    }

    inline DurationT restart()
    {
        const DurationT period(get_elapsed_time());
        time_point_ = now();
        return period;
    }

private:
    TimePoint time_point_;
};

class TimeBase
{
public:
    using HRCTimePoint = std::chrono::high_resolution_clock::time_point;
    using TimeStamp = std::chrono::duration<long, std::ratio<1, 1000000000>>;

    static inline void start() { s_start_time = std::chrono::high_resolution_clock::now(); }
    static inline void sync(HRCTimePoint time_point) { s_start_time = time_point; }
    static inline HRCTimePoint get_start_time() { return s_start_time; }
    static inline TimeStamp timestamp() { return std::chrono::high_resolution_clock::now() - s_start_time; }

private:
    static HRCTimePoint s_start_time;
};

using nanoClock = Clock<std::chrono::nanoseconds>;
using microClock = Clock<std::chrono::microseconds>;
using milliClock = Clock<std::chrono::milliseconds>;

} // namespace kb
