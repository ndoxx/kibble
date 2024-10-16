#pragma once

#include "kibble/logger/formatter.h"

namespace kb::log
{

/**
 * @brief Simple monochrome terminal formatter for VSCode's embedded terminal
 *
 * The source location information is displayed in such a way that
 * the user can ctrl+click on paths to jump to the exact code line
 * that triggered the log.
 *
 */
class MonochromeTerminalFormatter : public Formatter
{
public:
    ~MonochromeTerminalFormatter() = default;

    void print(const LogEntry&, const ChannelPresentation&) override;
};

} // namespace kb::log