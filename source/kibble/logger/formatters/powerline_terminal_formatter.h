#pragma once

#include "kibble/logger/formatter.h"

namespace kb::log
{

/**
 * @brief A powerline-styled VSCode terminal formatter
 *
 */
class PowerlineTerminalFormatter : public Formatter
{
public:
    ~PowerlineTerminalFormatter() = default;

    void print(const LogEntry&, const ChannelPresentation&) override;
};

} // namespace kb::log