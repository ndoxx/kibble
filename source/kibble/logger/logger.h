#pragma once

#include "common.h"

// LOGGING_ENABLED is defined via CMake
// The following lines make my code linter-friendly
#if LOGGING_ENABLED == 1
#define DO_LOG true
#else
#define DO_LOG false
#endif

namespace kb
{
namespace log_deprec
{

/**
 * @brief Output stream that will synchronize with the logger thread queue on std::endl.
 * This behavior is obtained through the use of a custom internal string buffer. Multiple thread_local logger streams
 * will exist, allowing concurrent access to the logging interface. Only the dispatch calls will introduce a
 * synchronization point. This way I can be sure that two concurrent accesses will never push intermingled garbage to
 * the sinks.
 *
 */
class LoggerStream : public std::ostream
{
private:
    // Custom stringbuf to react to std::endl
    class StringBuffer : public std::stringbuf
    {
    public:
        explicit StringBuffer(LoggerStream &parent);
        ~StringBuffer();

        // This override allows to monitor std::endl
        virtual int sync() override;
        // Let parent submit the completed message with some state, and clear buffer
        inline void submit()
        {
            parent_.submit(str());
            str("");
        }

    private:
        LoggerStream &parent_;
    };

public:
    /**
     * @brief Construct a new Logger Stream object. This constructor should not be called by the client code. The
     * get_log() function already declares logger streams with thread_local linkage, these are the only logger streams
     * you will ever need!
     *
     */
    LoggerStream();

    /**
     * @brief Destroy the Logger Stream object after it has emptied itself in the dispatcher.
     *
     */
    ~LoggerStream();

    /**
     * @brief Initialize log message state attributes.
     * The next logging statement that will be issued by this logger stream will inherit from all this state.
     *
     * @param channel Target channel
     * @param msg_type Type of message to send
     * @param severity Severity level of the message. The higher the severity, the lower the channel verbosity needs to
     * be for the message to be displayed.
     * @param code_line Source code line number that emitted this statement
     * @param code_file Source file that emitted this statement
     */
    void prepare(hash_t channel, MsgType msg_type, uint8_t severity, int code_line, const char *code_file);

private:
    /**
     * @internal
     * @brief Send message and current state to the dispatcher
     *
     * @param message Content of the logging statement
     */
    void submit(const std::string &message);

    StringBuffer buffer_; // The buffer this stream writes to
    LogStatement stmt_;   // Current state
};

/**
 * @brief Return a unique stream per invoker thread, using thread local storage
 *
 * @param channel Target channel
 * @param msg_type Type of message to send
 * @param severity Severity level of the message. The higher the severity, the lower the channel verbosity needs to
 * be for the message to be displayed.
 * @param code_line Source code line number that emitted this statement
 * @param code_file Source file that emitted this statement
 * @return LoggerStream& The thread_local instance of the logger stream
 */
[[maybe_unused]] static inline LoggerStream &get_log(hash_t channel, MsgType msg_type, uint8_t severity,
                                                     int code_line = 0, const char *code_file = "")
{
    thread_local LoggerStream ls;
    ls.prepare(channel, msg_type, severity, code_line, code_file);
    return ls;
}

} // namespace log_deprec

/**
 * @def KLOG(C, S)
 * Return the thread_local instance of the logger stream, and prepare() it to send a *normal* message with severity S to
 * channel C. When the symbol LOGGING_ENABLED is set to 0, this macro will be optimized out, and the arguments to the
 * stream operator will not be evaluated.\n
 * Example:\n
 * `KLOG("core",2) << "some message" << std::endl;`
 *
 * @def KLOGI
 * Send a message as an *item* of the previous message. It will keep the destination channel and severity, and will be
 * indented when displayed. Such messages are used when multiple aspects of the same thing must be enumerated and
 * displayed in order, this makes the output a bit more organized.\n Example:\n `KLOGI << "this is an item" <<
 * std::endl;`
 *
 * @def KLOGR(c)
 * Send a *raw* (unstyled) message to channel C with minimal severity. Raw messages are intended to be used whenever the
 * logger's style gets in the way, for example, when the message string itself contains custom ANSI code that could
 * interfere with the styling system. Another possible use is with special sinks like NetSink that may need to receive
 * unformatted input for the sake of the application.
 *
 * @def KLOGN(c)
 * Send a *notification* type message to channel C. The severity will be 1. Notifications are cues that something
 * (expected) has happened.
 *
 * @def KLOGW(c)
 * Send a *warning* type message to channel C. The severity will be 1. Warnings are cues that something less expected
 * has happened, and that maybe something needs to be done in order to sanitize the program state.
 *
 * @def KLOGE(c)
 * Send an *error* type message to channel C. The severity will be 2. Errors are cues that something bad but recoverable
 * has happened. An error may or may not leave the program in a valid state however.
 *
 * @def KLOGF(c)
 * Send a *fatal error* type message to channel C. The severity will be 3. Fatal errors are cues that something terrible
 * has happened, and that the program needs to exit this instant because there is nothing on Earth that can be done at
 * this point.
 *
 * @def KLOGG(C)
 * Cue the user that something good has happened.
 *
 * @def KLOGB(C)
 * Cue the user that something bad has happened. This is not to be used as an error message, it just means that
 * something failed, but there is a fallback in place that will be leveraged.
 *
 * @def KBANG()
 * Prints `BANG` in vivid orange, with the source line and file. Allows quick printf-debugging for lazy people like me.
 * It really draws attention, even drowned in a massive log.
 *
 * @def KLOGR__(C)
 * Like KLOGR() but is not disabled when LOGGING_ENABLED is set to zero.
 *
 */

// These macros will be optimized out (and arguments not evaluated)
// when LOGGING_ENABLED is set to 0. Should be better performance wise than using a null stream.
#define KLOG(C, S)                                                                                                     \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::log_deprec::MsgType::NORMAL, (S))

#define KLOGI                                                                                                          \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(0, kb::log_deprec::MsgType::ITEM, 4)

#define KLOGR(C)                                                                                                       \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::log_deprec::MsgType::RAW, 0)

#define KLOGN(C)                                                                                                       \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::log_deprec::MsgType::NOTIFY, 1)

#define KLOGW(C)                                                                                                       \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::log_deprec::MsgType::WARNING, 1, __LINE__, __FILE__)

#define KLOGE(C)                                                                                                       \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::log_deprec::MsgType::ERROR, 2, __LINE__, __FILE__)

#define KLOGF(C)                                                                                                       \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::log_deprec::MsgType::FATAL, 3, __LINE__, __FILE__)

#define KLOGG(C)                                                                                                       \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::log_deprec::MsgType::GOOD, 1, __LINE__, __FILE__)

#define KLOGB(C)                                                                                                       \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::log_deprec::MsgType::BAD, 1, __LINE__, __FILE__)

#define KBANG()                                                                                                        \
    if constexpr (!DO_LOG)                                                                                             \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_("core"), kb::log_deprec::MsgType::BANG, 3) << __FILE__ << ":" << __LINE__ << std::endl

#define KLOGR__(C) get_log(H_((C)), kb::log_deprec::MsgType::RAW, 0)

} // namespace kb
