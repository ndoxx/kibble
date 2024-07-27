#include "time/instrumentation.h"
#include "config.h"
#include "fmt/core.h"
#include "fmt/std.h"
#include <fstream>

namespace kb
{

InstrumentationSession::InstrumentationSession()
    : base_timestamp_us_(
          std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now())
              .time_since_epoch()
              .count())
{
    profile_data_.resize(KIBBLE_JOBSYS_MAX_THREADS);
}

void InstrumentationSession::push(const ProfileResult& result)
{
    if (enabled_)
    {
        // Each thread has its own queue, no need to lock.
        profile_data_[result.thread_id].push_back(result);
    }
}

void InstrumentationSession::push(ProfileResult&& result)
{
    if (enabled_)
    {
        profile_data_[result.thread_id].push_back(std::move(result));
    }
}

void InstrumentationSession::write(const fs::path& filepath)
{
    std::ofstream ofs(filepath);

    // FIX: Global locale may have changed, we want to make sure we use the C locale,
    // this avoids outputting float values with comma decimal separators for instance...
    std::locale system_locale("C");
    ofs.imbue(system_locale);

    // Trace Event Format:
    // https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/edit

    fmt::print(ofs, "{{\"otherData\": {{}},\"traceEvents\":[");

    size_t profile_count = 0;
    for (const auto& profiles : profile_data_)
    {
        for (const auto& profile : profiles)
        {
            std::string name = profile.name;
            std::replace(name.begin(), name.end(), '"', '\'');
            if (profile_count++ > 0)
            {
                fmt::print(ofs, ",");
            }
            fmt::print(ofs, "{{\"cat\":\"{}\",\"dur\":{},\"name\":\"{}\",\"ph\":\"X\",\"pid\":0,\"tid\":{},\"ts\":{}}}",
                       profile.category, profile.end - profile.start, name, profile.thread_id + 1,
                       profile.start - base_timestamp_us_);
        }
    }

    fmt::print(ofs, "]}}");
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
    {
        return;
    }

    auto stop = std::chrono::high_resolution_clock::now();
    long long start_us = std::chrono::time_point_cast<std::chrono::microseconds>(start_).time_since_epoch().count();
    long long stop_us = std::chrono::time_point_cast<std::chrono::microseconds>(stop).time_since_epoch().count();

    session_->push({name_, category_, thread_id_, start_us, stop_us});
}

} // namespace kb