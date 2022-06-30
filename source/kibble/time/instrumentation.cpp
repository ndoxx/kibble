#include "time/instrumentation.h"
#include "logger/logger.h"

namespace kb
{

InstrumentationSession::InstrumentationSession(const fs::path &filepath) : stream_(filepath)
{
    try
    {
        write_header();
    }
    catch (std::exception &e)
    {
        KLOGE("core") << "An exception occurred while trying to write profiling header:" << std::endl;
        KLOGI << e.what() << std::endl;
    }
}

InstrumentationSession::~InstrumentationSession()
{
    try
    {
        write_footer();
        stream_.close();
    }
    catch (std::exception &e)
    {
        KLOGE("core") << "An exception occurred while trying to write profiling footer:" << std::endl;
        KLOGI << e.what() << std::endl;
    }
}

void InstrumentationSession::write_header()
{
    stream_ << "{\"otherData\": {},\"traceEvents\":[";
    stream_.flush();
}

void InstrumentationSession::write_profile(const ProfileResult &result)
{
    if (!enabled_)
        return;

    if (profile_count_++ > 0)
        stream_ << ",";

    std::string name = result.name;
    std::replace(name.begin(), name.end(), '"', '\'');

    // clang-format off
    stream_ << "{"
            << "\"cat\":\"" << result.category << "\","
            << "\"dur\":" << (result.end - result.start) << ',' 
            << "\"name\":\"" << name << "\","
            << "\"ph\":\"X\","
            << "\"pid\":0,"
            << "\"tid\":" << result.thread_id << ','
            << "\"ts\":" << result.start << '}';
    // clang-format on

    stream_.flush();
}

void InstrumentationSession::write_footer()
{
    stream_ << "]}";
    stream_.flush();
}

InstrumentationTimer::InstrumentationTimer(InstrumentationSession &session, const std::string &name,
                                           const std::string &category)
    : session_(session), name_(name), category_(category), start_(std::chrono::high_resolution_clock::now())
{
}

InstrumentationTimer::~InstrumentationTimer()
{
    auto stop = std::chrono::high_resolution_clock::now();
    long long start_us = std::chrono::time_point_cast<std::chrono::microseconds>(start_).time_since_epoch().count();
    long long stop_us = std::chrono::time_point_cast<std::chrono::microseconds>(stop).time_since_epoch().count();

    try
    {
        session_.write_profile({name_, category_, std::this_thread::get_id(), start_us, stop_us});
    }
    catch (std::exception &e)
    {
        KLOGE("core") << "An exception occurred while trying to write profiling data:" << std::endl;
        KLOGI << e.what() << std::endl;
    }
}

} // namespace kb