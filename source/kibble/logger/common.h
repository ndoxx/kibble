#pragma once

#include <array>
#include <sstream>

#include "../assert/assert.h"
#include "../hash/hash.h"
#include "../math/color_table.h"
#include "../time/clock.h"

namespace kb
{

/**
 * @brief When such an object is passed to a logger stream, the style is reset to default.
 */
struct ConsoleColorClear
{
    friend std::ostream &operator<<(std::ostream &, const ConsoleColorClear &);
};

/**
 * @brief A console color modifier that changes the current log color.
 * When such an object is passed to a logger stream, either the foreground or the background color will be changed,
 * depending on the template parameter value.
 *
 * @tparam FOREGROUND if true, affects the foreground color, if false, affects the background color
 */
template <bool FOREGROUND>
struct ConsoleColor
{
    /**
     * @brief Default constructor will generate a white color
     */
    constexpr ConsoleColor() : color_{0xFFFFFF}
    {
    }

    /**
     * @brief Construct a console color modifier using an argb32 representation of a color.
     * @see math::argb32_t
     */
    constexpr ConsoleColor(math::argb32_t argb) : color_(argb)
    {
    }

    /**
     * @brief Construct a color modifier from three red, green and blue values
     *
     */
    constexpr ConsoleColor(uint8_t R, uint8_t G, uint8_t B) : color_(math::pack_ARGB(R, G, B))
    {
    }

    friend std::ostream &operator<<(std::ostream &, const ConsoleColor &);
    math::argb32_t color_; /// Holds the color as a 32 bits argb value
};

#define KF_ ConsoleColor<true>
#define KB_ ConsoleColor<false>
#define KC_                                                                                                            \
    ConsoleColorClear                                                                                                  \
    {                                                                                                                  \
    }

// Semiotics of colors I'm used to
/// @def KS_PATH_ Console color for: path
#define KS_PATH_ KF_(kb::col::cyan)        
/// @def KS_INST_ Console color for: instruction
#define KS_INST_ KF_(kb::col::lightorange) 
/// @def KS_DEFL_ Console color for: default
#define KS_DEFL_ KF_(kb::col::ndxorange)   
/// @def KS_NAME_ Console color for: name
#define KS_NAME_ KF_(kb::col::orangered)   
/// @def KS_IVAL_ Console color for: important value
#define KS_IVAL_ KF_(kb::col::violet)      
/// @def KS_VALU_ Console color for: value
#define KS_VALU_ KF_(kb::col::lawngreen)   
/// @def KS_ATTR_ Console color for: attribute
#define KS_ATTR_ KF_(kb::col::lime)        
/// @def KS_NODE_ Console color for: node
#define KS_NODE_ KF_(kb::col::turquoise)   
/// @def KS_HIGH_ Console color for: highlight
#define KS_HIGH_ KF_(kb::col::pink)        
/// @def KS_POS_ Console color for: good thing
#define KS_POS_ KF_(kb::col::green)       
/// @def KS_NEG_ Console color for: bad thing
#define KS_NEG_ KF_(kb::col::red)          

namespace log_deprec
{

/**
 * @brief Enumerates all possible message types that are displayable by the logger.
 *
 */
enum class MsgType : std::uint8_t
{
    /// Raw message, no decoration
    RAW,
    /// No effect white message
    NORMAL,
    /// Item in list
    ITEM,
    /// For event tracking
    EVENT,
    /// Relative to an event which should be notified to the user
    NOTIFY,
    /// Relative to an event which could impact the flow badly
    WARNING,
    /// Relative to a serious but recoverable error (eg. missing texture when you have a fall back one)
    ERROR,
    /// Relative to non recoverable error (eg. out of memory...)
    FATAL,
    /// For code flow analysis
    BANG,
    /// For test success
    GOOD,
    /// For test fail
    BAD,
    /// The total number of message types
    COUNT
};

/**
 * @brief Represents a single logging statement.
 *
 */
struct LogStatement
{
    /// The channel targeted by this statement
    hash_t channel;                
    /// Type of message
    MsgType msg_type;              
    /// Time point at which the message was logged
    TimeBase::TimeStamp timestamp; 
    /// The higher the severity, the lower the channel verbosity needs to be for the message to be displayed
    uint8_t severity;              
    /// Source code line number that emitted this statement
    int code_line;                 
    /// Source file that emitted this statement
    std::string code_file;         
    /// String message content
    std::string message;           
};

/**
 * @brief Represents a logging channel.
 * Logging channels are used to group messages emanating from the same subsystems. Messages coming from different
 * channels are styled differently, for a better reading experience. Each channel has a verbosity setting that allows to
 * filter out messages under a given severity level.
 *
 */
struct LogChannel
{
    /// Verbosity level. The lower it gets, the higher the message severity needs to be to avoid filtering.
    uint8_t verbosity; 
    /// Full name of the channel
    std::string name;  
    /// styled label used by the console sink
    std::string tag;   
};

/**
 * @brief Contains arrays with the various styles used for each message type
 *
 */
class Style
{
public:
    static const std::array<KF_, size_t(MsgType::COUNT)> s_colors;
    static const std::array<std::string, size_t(MsgType::COUNT)> s_icons;
};

} // namespace log_deprec
} // namespace kb