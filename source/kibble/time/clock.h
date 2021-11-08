#pragma once

#include <chrono>

namespace kb
{

/**
 * @brief High precision chronometer.
 *
 * @tparam DurationT duration type
 */
template <typename DurationT>
class Clock
{
public:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock, DurationT>;

    /**
     * @brief Get the current time point.
     *
     * @return TimePoint
     */
    static inline TimePoint now()
    {
        return std::chrono::time_point_cast<DurationT>(std::chrono::high_resolution_clock::now());
    }

    /**
     * @brief Start the chronometer as the object is constructed.
     *
     */
    Clock() : time_point_(now())
    {
    }

    /**
     * @brief Get time elapsed since the chronometer was constructed or restarted.
     *
     * @return DurationT
     */
    inline DurationT get_elapsed_time() const
    {
        return now() - time_point_;
    }

    /**
     * @brief Restart the chronometer and return the elapsed time.
     *
     * @return DurationT
     */
    inline DurationT restart()
    {
        const DurationT period(get_elapsed_time());
        time_point_ = now();
        return period;
    }

private:
    TimePoint time_point_;
};

/**
 * @brief Time stamp factory.
 * This static class is used to generate timestamps relative to the timepoint taken after a call to start().
 *
 */
class TimeBase
{
public:
    using HRCTimePoint = std::chrono::high_resolution_clock::time_point;
    using TimeStamp = std::chrono::duration<long, std::ratio<1, 1000000000>>;

    /**
     * @brief Manually start the clock.
     * The start time point is statically initialized, but when calling this function it is set again.
     * @todo Rename to restart() which is a better fit.
     *
     */
    static inline void start()
    {
        s_start_time = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Change the time point of this TimeBase, to sync with another TimeBase.
     * This may be needed in cases where a dependency also uses this class.
     *
     * @param time_point new time point
     */
    static inline void sync(HRCTimePoint time_point)
    {
        s_start_time = time_point;
    }

    /**
     * @brief Return the base time point
     *
     * @return HRCTimePoint
     */
    static inline HRCTimePoint get_start_time()
    {
        return s_start_time;
    }

    /**
     * @brief Generate a timestamp, relative to the start time point
     *
     * @return TimeStamp
     */
    static inline TimeStamp timestamp()
    {
        return std::chrono::high_resolution_clock::now() - s_start_time;
    }

private:
    static HRCTimePoint s_start_time;
};

using nanoClock = Clock<std::chrono::nanoseconds>;
using microClock = Clock<std::chrono::microseconds>;
using milliClock = Clock<std::chrono::milliseconds>;

} // namespace kb
