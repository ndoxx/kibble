#pragma once

#include <array>
#include <sstream>

#include "../assert/assert.h"
#include "../hash/hashstr.h"
#include "../time/clock.h"

namespace kb
{

struct ConsoleColorClear
{
    friend std::ostream& operator<<(std::ostream&, const ConsoleColorClear&);
};

template <bool FOREGROUND> struct ConsoleColor
{
    ConsoleColor(): color_(0xFFFFFF) {}
    explicit ConsoleColor(char shorthand)
    {
        switch(shorthand)
        {
            case 'w': color_ = 0xFFFFFF; break; // White
            case 'k': color_ = 0x000000; break; // blacK
            case 'r': color_ = 0xFF0000; break; // Red
            case 'g': color_ = 0x00FF00; break; // Green
            case 'b': color_ = 0x0000FF; break; // Blue
            case 'c': color_ = 0x00FFFF; break; // Cyan
            case 'd': color_ = 0xFF3200; break; // Dark orange
            case 'o': color_ = 0xFF6400; break; // Orange
            case 's': color_ = 0xFFBE00; break; // light orange (Sand)
            case 'v': color_ = 0xCC00CC; break; // Violet
            case 't': color_ = 0x00CED1; break; // Turquoise
            case 'p': color_ = 0xFF33CC; break; // Pink
            case 'l': color_ = 0x00FF64; break; // Lime
            case 'a': color_ = 0x99CC00; break; // lAwn green

            default: K_ASSERT_FMT(false, "Unknown shorthand color notation: %c", shorthand);
        }
    }
    ConsoleColor(uint8_t R, uint8_t G, uint8_t B);

    friend std::ostream& operator<<(std::ostream&, const ConsoleColor&);
    uint32_t color_;
};

template<> ConsoleColor<true>::ConsoleColor(uint8_t R, uint8_t G, uint8_t B);
template<> ConsoleColor<false>::ConsoleColor(uint8_t R, uint8_t G, uint8_t B);

#define KF_ ConsoleColor<true>
#define KB_ ConsoleColor<false>
#define KC_ ConsoleColorClear{}

#define KS_PATH_ KF_('c')
#define KS_INST_ KF_('s')
#define KS_DEFL_ KF_('o')
#define KS_NAME_ KF_('d')
#define KS_IVAL_ KF_('v')
#define KS_VALU_ KF_('a')
#define KS_ATTR_ KF_('l')
#define KS_NODE_ KF_('t')
#define KS_HIGH_ KF_('p')
#define KS_GOOD_ KF_('g')
#define KS_BAD_  KF_('r')

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