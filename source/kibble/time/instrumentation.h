#pragma once

// Adapted from https://gist.github.com/TheCherno/31f135eea6ee729ab5f26a6908eb3a5e
// Basic instrumentation profiler by Cherno

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace kb
{

/**
 * @brief All the information necessary to write an execution profile.
 *
 */
struct ProfileResult
{
    /// Descriptive name of the function / scope / task
    std::string name;
    /// Event type, to allow for filtering in chrome:tracing
    std::string category;
    /// ID of the thread that executed the function
    size_t thread_id;
    /// Start timestamp in µs
    long long start;
    /// Stop timestamp in µs
    long long end;
};

/**
 * @brief Encapsulates profile logging facilities for some part of your codebase.
 * In a game engine, the startup, runtime and shutdown will typically correspond
 * to three distinct profiling sessions. This makes it easier to find the relevant
 * information later on.
 *
 */
class InstrumentationSession
{
public:
    /**
     * @brief Construct session.
     *
     * @param filepath Output json file path.
     */
    InstrumentationSession();

    /**
     * @brief Save profiling information.
     *
     * @note Lock-free and thread-safe.
     *
     * @param result Profiling data.
     */
    void push(const ProfileResult& result);
    void push(ProfileResult&& result);

    /**
     * @brief Write profiling information to file.
     *
     * @param filepath
     */
    void write(const fs::path& filepath);

    /**
     * @brief Turn on / off profiling for this session.
     *
     * @param value
     */
    inline void enable(bool value)
    {
        enabled_ = value;
    }

private:
    const long long base_timestamp_us_;
    bool enabled_ = true;
    std::vector<std::vector<ProfileResult>> profile_data_;
};

/**
 * @brief Utility class to facilitate function profiling.
 * When this object is created, a timer starts, and when it is destroyed,
 * the timer stops and all relevant information is sent to the instrumentation
 * session.
 *
 * @note You can furthermore define macros to simplify the declaration (see example).
 *
 */
class InstrumentationTimer
{
public:
    /**
     * @brief Start timer on construction.
     *
     * @param session The instrumentation session this timer will send information to on destruction.
     * @param name Name of the current work unit (displayed in chrome:tracing)
     * @param category Event type, to allow for filtering in chrome:tracing
     */
    InstrumentationTimer(InstrumentationSession* session, const std::string& name, const std::string& category,
                         size_t thread_id = 0);

    /**
     * @brief Stop timer, and send execution information to the session.
     *
     */
    ~InstrumentationTimer();

private:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    InstrumentationSession* session_;
    std::string name_;
    std::string category_;
    size_t thread_id_;
    TimePoint start_;
};

} // namespace kb
