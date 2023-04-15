#include "instrumentation.h"
#include <fstream>

namespace kb
{

InstrumentationSession::InstrumentationSession()
    : base_timestamp_us_(
          std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now())
              .time_since_epoch()
              .count())
{
}

void InstrumentationSession::push(const ProfileResult& result)
{
    if (!enabled_)
        return;

    profile_data_[result.thread_id].push_back(result);
}

void InstrumentationSession::write(const fs::path& filepath)
{
    std::ofstream ofs(filepath);

    ofs << "{\"otherData\": {},\"traceEvents\":[";

    size_t profile_count = 0;
    for (const auto& profiles : profile_data_)
    {
        for (const auto& profile : profiles)
        {
            if (profile_count++ > 0)
                ofs << ",";

            std::string name = profile.name;
            std::replace(name.begin(), name.end(), '"', '\'');

            // clang-format off
            ofs << "{"
                << "\"cat\":\"" << profile.category << "\","
                << "\"dur\":" << (profile.end - profile.start) << ',' 
                << "\"name\":\"" << name << "\","
                << "\"ph\":\"X\","
                << "\"pid\":0,"
                << "\"tid\":" << profile.thread_id + 1 << ','
                << "\"ts\":" << profile.start - base_timestamp_us_ << '}';
            // clang-format on
        }
    }

    ofs << "]}" << std::endl;
}

InstrumentationTimer::InstrumentationTimer(InstrumentationSession* session, const std::string& name,
                                           const std::string& category, size_t thread_id)
    : session_(session), name_(name), category_(category), thread_id_(thread_id),
      start_(std::chrono::high_resolution_clock::now())
{
}

InstrumentationTimer::~InstrumentationTimer()
{
    if (session_ == nullptr)
        return;

    auto stop = std::chrono::high_resolution_clock::now();
    long long start_us = std::chrono::time_point_cast<std::chrono::microseconds>(start_).time_since_epoch().count();
    long long stop_us = std::chrono::time_point_cast<std::chrono::microseconds>(stop).time_since_epoch().count();

    session_->push({name_, category_, thread_id_, start_us, stop_us});
}

} // namespace kb