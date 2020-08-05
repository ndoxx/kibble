#pragma once

#include <array>
#include <sstream>

#include "../hash/hashstr.h"
#include "../time/time_base.h"

#if LOGGING_ANSI_3 == 1
#define ANSI_3 true
#else
#define ANSI_3 false
#endif

namespace kb
{

// WCore Console Color
// Evaluates to an ansi string when submitted to a stream
struct WCC
{
    WCC() = default;
    explicit WCC(char cc);
    explicit WCC(const std::string&);
    WCC(uint8_t R, uint8_t G, uint8_t B);

    friend std::ostream& operator<<(std::ostream& stream, const WCC& wcc);

    std::string escape;
};

// WCore Console Background color
// Evaluates to an ansi string when submitted to a stream
struct WCB
{
    WCB() = default;
    explicit WCB(int cc);
    explicit WCB(const std::string&);
    WCB(uint8_t R, uint8_t G, uint8_t B);

    friend std::ostream& operator<<(std::ostream& stream, const WCB& wcc);

    std::string escape;
};

namespace klog
{

enum class MsgType : std::uint8_t
{
    RAW,     // Raw message, no decoration
    NORMAL,  // No effect white message
    ITEM,    // Item in list
    EVENT,   // For event tracking
    NOTIFY,  // Relative to an event which should be notified to the user
    WARNING, // Relative to an event which could impact the flow badly
    ERROR,   // Relative to a serious but recoverable error
             // (eg. missing texture when you have a fall back one)
    FATAL,   // Relative to non recoverable error (eg. out of memory...)
    BANG,    // For code flow analysis
    GOOD,    // For test success
    BAD,     // For test fail

    COUNT
};

struct LogStatement
{
    hash_t channel;
    MsgType msg_type;
    TimeStamp timestamp;
    uint8_t severity;
    int code_line;
    std::string code_file;
    std::string message;
};

struct LogChannel
{
    uint8_t verbosity;
    std::string name;
    std::string tag;
};

class Style
{
public:
    static const std::array<WCC, size_t(MsgType::COUNT)> s_colors;
    static const std::array<std::string, size_t(MsgType::COUNT)> s_icons;
};

} // namespace klog
} // namespace kb