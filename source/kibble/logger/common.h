#pragma once

#include <array>
#include <sstream>

#include "../assert/assert.h"
#include "../hash/hash.h"
#include "../math/color_table.h"
#include "../time/clock.h"

namespace kb
{

struct ConsoleColorClear
{
    friend std::ostream& operator<<(std::ostream&, const ConsoleColorClear&);
};

template <bool FOREGROUND> struct ConsoleColor
{
    constexpr ConsoleColor() : color_{0xFFFFFF} {}
    constexpr ConsoleColor(math::argb32_t argb) : color_(argb) {}
    constexpr ConsoleColor(uint8_t R, uint8_t G, uint8_t B) : color_(math::pack_ARGB(R, G, B)) {}

    friend std::ostream& operator<<(std::ostream&, const ConsoleColor&);
    math::argb32_t color_;
};

#define KF_ ConsoleColor<true>
#define KB_ ConsoleColor<false>
#define KC_ ConsoleColorClear{}

// Semiotics of colors I'm used to
#define KS_PATH_ KF_(kb::col::cyan)        // path
#define KS_INST_ KF_(kb::col::lightorange) // instruction
#define KS_DEFL_ KF_(kb::col::ndxorange)   // default
#define KS_NAME_ KF_(kb::col::orangered)   // name
#define KS_IVAL_ KF_(kb::col::violet)      // important value
#define KS_VALU_ KF_(kb::col::lawngreen)   // value
#define KS_ATTR_ KF_(kb::col::lime)        // attribute
#define KS_NODE_ KF_(kb::col::turquoise)   // node
#define KS_HIGH_ KF_(kb::col::pink)        // highlight
#define KS_GOOD_ KF_(kb::col::green)       // good thing
#define KS_BAD_  KF_(kb::col::red)         // bad thing

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
    TimeBase::TimeStamp timestamp;
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
    static const std::array<KF_, size_t(MsgType::COUNT)> s_colors;
    static const std::array<std::string, size_t(MsgType::COUNT)> s_icons;
};

} // namespace klog
} // namespace kb