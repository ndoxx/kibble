#include "kibble/time/instrumentation.h"

#include <thread>

using namespace kb;

// The instrumentation session is made global for ease of use, but it's not mandatory.
InstrumentationSession* session = nullptr;

// The following macros simplify the declaration of an instrumentation timer.
// The token pasting stuff allows to declare multiple timers with
// different names in the same function.
#define CONCAT_IMPL(first, second) first##second
#define CONCAT(first, second) CONCAT_IMPL(first, second)
#define PROFILE_SCOPE(name, category) InstrumentationTimer CONCAT(timer_, __LINE__)(session, name, category)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__PRETTY_FUNCTION__, "function")

// This function will be profiled
void test_func_01(size_t ms)
{
    PROFILE_FUNCTION();
    // Do some work
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// This function will be profiled as well
void test_func_nested()
{
    PROFILE_FUNCTION();
    // Do some work
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // We can profile multiple scopes within this function
    {
        PROFILE_SCOPE("Work Unit #1", "physics");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    {
        PROFILE_SCOPE("Work Unit #2", "game logic");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
        PROFILE_SCOPE("Work Unit #3", "AI");
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
}

// This function will call the previous function
void test_func_02()
{
    PROFILE_FUNCTION();
    // Do some work
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // Call another function
    test_func_nested();
    // Do some work
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

/**
 * @brief The following program shows basic instrumentation timer usage.
 * Two functions with different profiling granularities are executed sequentially,
 * and their execution time is recorded in a json file that you can open
 * with the chrome::tracing tool. This tool allow to easily visualize what
 * happens and when in your program.
 *
 * @return int
 */
int main(int, char**)
{
    // Create an instrumentation session
    session = new InstrumentationSession();

    // Call the test functions multiple times
    for (size_t ii = 0; ii < 10; ++ii)
    {
        test_func_01(ii);
        test_func_02();
    }

    session->write("example_profile.json");

    delete session;
    return 0;
}