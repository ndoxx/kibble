/**
 * @file assert.h
 * @brief Better assertions.
 *
 * Adapted from https://github.com/planetchili/Chil/blob/master/Core/src/utl/Assert.h
 *
 * The macros in this file make assertions more useful. A source file and code line will be printed when an assertion
 * fails, and variable values can be tracked.
 *
 * @author ndx
 * @version 0.2
 * @date 2023-04-06
 *
 */

#pragma once

#include <sstream>
#include <string>

/**
 * @def K_DEBUG_BREAK()
 * Break into debugger.
 *
 * @def K_ASSERT(EXPR, MSG, CHAN)
 * Assert that an expression EXPR evaluates to true. If it not, log an error message MSG on channel CHAN with some
 * contextual information.
 *
 */

// Macro to break into debugger
#ifdef K_DEBUG
#ifdef __linux__
#include <csignal>
#define K_DEBUG_BREAK() raise(SIGTRAP)
#endif
#ifdef _WIN32
#define K_DEBUG_BREAK() __debugbreak()
#endif
#else
#define K_DEBUG_BREAK()
#endif

namespace kb::log
{
class Channel;
}

namespace kb::util
{

class Assertion
{
public:
    enum class Trigger
    {
        Log,
        Terminate,
        Exception,
    };

    Assertion(std::string expression, const kb::log::Channel *channel, const char *file, const char *function, int line,
              Trigger trig = Trigger::Terminate);

    ~Assertion();

    inline Assertion &msg(const std::string &message)
    {
        stream_ << "    Msg: " << message << '\n';
        return *this;
    }

    template <typename T>
    inline Assertion &watch_var__(T &&val, const char *name)
    {
        stream_ << "    " << name << " <- " << std::forward<T>(val) << '\n';
        return *this;
    }

    [[noreturn]] void ex();

private:
    const kb::log::Channel *channel_ = nullptr;
    const char *file_;
    const char *function_;
    int line_ = -1;
    Trigger trigger_;
    std::ostringstream stream_;
};

} // namespace kb::util

#ifndef K_ENABLE_ASSERT
#ifdef K_DEBUG
#define K_ENABLE_ASSERT true
#else
#define K_ENABLE_ASSERT false
#endif
#endif

#define ZZ_STR(x) #x
#define K_ASSERT(EXPR, MSG, CHAN)                                                                                      \
    (!K_ENABLE_ASSERT || bool(EXPR))                                                                                   \
        ? void(0)                                                                                                      \
        : (void)kb::util::Assertion{ZZ_STR(EXPR), CHAN, __FILE__, __PRETTY_FUNCTION__, __LINE__}.msg(MSG)

#define K_CHECK(EXPR, MSG, CHAN)                                                                                       \
    bool(EXPR)                                                                                                         \
        ? void(0)                                                                                                      \
        : (void)kb::util::                                                                                             \
              Assertion{ZZ_STR(EXPR),                                                                                  \
                        CHAN,                                                                                          \
                        __FILE__,                                                                                      \
                        __PRETTY_FUNCTION__,                                                                           \
                        __LINE__,                                                                                      \
                        K_ENABLE_ASSERT ? kb::util::Assertion::Trigger::Terminate : kb::util::Assertion::Trigger::Log} \
                  .msg(MSG)

#define K_FAIL(CHAN)                                                                                                   \
    (void)kb::util::Assertion                                                                                          \
    {                                                                                                                  \
        "[Always Fail]", CHAN, __FILE__, __PRETTY_FUNCTION__, __LINE__,                                                \
            K_ENABLE_ASSERT ? kb::util::Assertion::Trigger::Terminate : kb::util::Assertion::Trigger::Log              \
    }

#define watch(EXPR) watch_var__((EXPR), ZZ_STR(EXPR))