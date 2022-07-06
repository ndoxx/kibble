#pragma once

// Adapted from https://gist.github.com/TheCherno/31f135eea6ee729ab5f26a6908eb3a5e
// Basic instrumentation profiler by Cherno

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>

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
    std::thread::id thread_id;
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
     * @brief Construct session and write header to stream.
     *
     * @param filepath Output json file path.
     */
    InstrumentationSession(const fs::path& filepath);

    /**
     * @brief Write footer to stream and destroy the session.
     *
     */
    ~InstrumentationSession();

    /**
     * @brief Write profiling information.
     *
     * @param result Profiling data.
     */
    void write_profile(const ProfileResult &result);

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
    /**
     * @internal
     * @brief Write the beginning of the json file.
     *
     */
    void write_header();

    /**
     * @internal
     * @brief Write the end of the json file.
     *
     */
    void write_footer();

private:
    const long long base_timestamp_us_;
    std::ofstream stream_;
    bool enabled_ = true;
    size_t profile_count_ = 0;
    std::mutex stream_mtx_;
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
    InstrumentationTimer(InstrumentationSession *session, const std::string &name, const std::string &category);

    /**
     * @brief Stop timer, and send execution information to the session.
     *
     */
    ~InstrumentationTimer();

private:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    InstrumentationSession *session_;
    std::string name_;
    std::string category_;
    TimePoint start_;
};

} // namespace kb