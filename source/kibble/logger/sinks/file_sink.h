#pragma once

#include "../../util/internal.h"
#include "../sink.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace kb::log
{

/**
 * @brief Direct all input log entries to a file.
 *
 * Formatting is done internally at the moment.
 *
 */
class FileSink : public Sink
{
public:
    /**
     * @brief Construct a new file sink that will log to the given input file
     *
     * @param filepath
     */
    FileSink(const fs::path& filepath);

    ~FileSink() = default;

    void submit(const struct LogEntry&, const struct ChannelPresentation&) override;

    void flush() const override;

private:
    fs::path filepath_;

    struct Internal;
    friend struct internal_deleter::Deleter<Internal>;
    internal_ptr<Internal> internal_ = nullptr;
};

} // namespace kb::log