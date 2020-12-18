#pragma once

#include <array>
#include <sstream>

#include "../assert/assert.h"
#include "../hash/hash.h"
#include "../math/color.h"
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
    constexpr ConsoleColor(uint8_t R, uint8_t G, uint8_t B): color_(math::pack_ARGB(R, G, B)) {}
    constexpr explicit ConsoleColor(char shorthand)
    {
        switch(shorthand)
        {
            case 'w': color_ = {0xFFFFFF}; break; // White
            case 'k': color_ = {0x000000}; break; // blacK
            case 'r': color_ = {0xFF0000}; break; // Red
            case 'g': color_ = {0x00FF00}; break; // Green
            case 'b': color_ = {0x0000FF}; break; // Blue
            case 'c': color_ = {0x00FFFF}; break; // Cyan
            case 'd': color_ = {0xFF3200}; break; // Dark orange
            case 'o': color_ = {0xFF6400}; break; // Orange
            case 's': color_ = {0xFFBE00}; break; // light orange (Sand)
            case 'v': color_ = {0xCC00CC}; break; // Violet
            case 't': color_ = {0x00CED1}; break; // Turquoise
            case 'p': color_ = {0xFF33CC}; break; // Pink
            case 'l': color_ = {0x00FF64}; break; // Lime
            case 'a': color_ = {0x99CC00}; break; // lAwn green

            default:  color_ = {0xFFFFFF}; break; // Default to white
        }
    }

    friend std::ostream& operator<<(std::ostream&, const ConsoleColor&);
    math::argb32_t color_;
};

#define KF_ ConsoleColor<true>
#define KB_ ConsoleColor<false>
#define KC_ ConsoleColorClear {}

// Semiotics of colors I'm used to
#define KS_PATH_ KF_('c') // path
#define KS_INST_ KF_('s') // instruction
#define KS_DEFL_ KF_('o') // default
#define KS_NAME_ KF_('d') // name
#define KS_IVAL_ KF_('v') // important value
#define KS_VALU_ KF_('a') // value
#define KS_ATTR_ KF_('l') // attribute
#define KS_NODE_ KF_('t') // node
#define KS_HIGH_ KF_('p') // highlight
#define KS_GOOD_ KF_('g') // good thing
#define KS_BAD_  KF_('r') // bad thing

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