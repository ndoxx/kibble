#pragma once
#include <string>

namespace kb::log
{

struct LogEntry;
struct ChannelPresentation;

/**
 * @brief Interface for a text formatter
 *
 */
class Formatter
{
public:
    virtual ~Formatter() = default;

    /**
     * @brief Override this with code that produces a formatted string
     *
     * @return std::string
     */
    virtual std::string format_string(const LogEntry&, const ChannelPresentation&)
    {
        return "";
    }

    /**
     * @brief Override this with code that produces a formatted console print
     *
     */
    virtual void print(const LogEntry&, const ChannelPresentation&)
    {
    }
};

} // namespace kb::log