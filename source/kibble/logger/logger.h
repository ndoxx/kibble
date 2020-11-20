#pragma once

#include "logger_common.h"

// LOGGING_ENABLED is defined via CMake
// The following lines make my code linter-friendly
#if LOGGING_ENABLED == 1
#define DO_LOG true
#else
#define DO_LOG false
#endif

namespace kb
{
namespace klog
{

// Output stream that will synchronize with the logger thread queue on std::endl
class LoggerStream : public std::ostream
{
private:
    // Custom stringbuf to react to std::endl
    class StringBuffer : public std::stringbuf
    {
    public:
        explicit StringBuffer(LoggerStream& parent);
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
        LoggerStream& parent_;
    };

public:
    LoggerStream();
    ~LoggerStream();

    // Initialize log message state attributes
    void prepare(hash_t channel, MsgType msg_type, uint8_t severity, int code_line, const char* code_file);

private:
    // Send message and current state to logger thread
    void submit(const std::string& message);

    StringBuffer buffer_; // The buffer this stream writes to
    LogStatement stmt_;   // Current state
};

// Return a unique stream per invoker thread, using thread local storage
[[maybe_unused]] static inline LoggerStream& get_log(hash_t channel, MsgType msg_type, uint8_t severity,
                                                     int code_line = 0, const char* code_file = "")
{
    thread_local LoggerStream ls;
    ls.prepare(channel, msg_type, severity, code_line, code_file);
    return ls;
}

} // namespace klog

// These macros will be optimized out (and arguments not evaluated)
// when LOGGING_ENABLED is set to 0. Should be better performance wise than using a null stream.
#define KLOG(C, S)                                                                                                     \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::klog::MsgType::NORMAL, (S))
#define KLOGI                                                                                                          \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(0, kb::klog::MsgType::ITEM, 4)
#define KLOGR(C)                                                                                                       \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::klog::MsgType::RAW, 0)
#define KLOGN(C)                                                                                                       \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::klog::MsgType::NOTIFY, 0)
#define KLOGW(C)                                                                                                       \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::klog::MsgType::WARNING, 1, __LINE__, __FILE__)
#define KLOGE(C)                                                                                                       \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::klog::MsgType::ERROR, 2, __LINE__, __FILE__)
#define KLOGF(C)                                                                                                       \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::klog::MsgType::FATAL, 3, __LINE__, __FILE__)
#define KLOGG(C)                                                                                                       \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::klog::MsgType::GOOD, 3, __LINE__, __FILE__)
#define KLOGB(C)                                                                                                       \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_((C)), kb::klog::MsgType::BAD, 3, __LINE__, __FILE__)
#define KBANG()                                                                                                        \
    if constexpr(!DO_LOG)                                                                                              \
        ;                                                                                                              \
    else                                                                                                               \
        get_log(H_("core"), kb::klog::MsgType::BANG, 3) << __FILE__ << ":" << __LINE__ << std::endl

#define KLOGR__(C) get_log(H_((C)), kb::klog::MsgType::RAW, 0)

} // namespace kb
