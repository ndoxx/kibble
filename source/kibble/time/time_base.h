#pragma once

#include <chrono>

namespace kb
{

typedef std::chrono::high_resolution_clock::time_point TimePoint;
typedef std::chrono::duration<long, std::ratio<1, 1000000000>> TimeStamp;

class TimeBase
{
public:
	static inline void start() { START_TIME = std::chrono::high_resolution_clock::now(); }
    static inline TimeStamp timestamp() { return std::chrono::high_resolution_clock::now() - START_TIME; }

private:
    static TimePoint START_TIME;
};

} // namespace kb