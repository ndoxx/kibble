#pragma once

#include <filesystem>
#include "../../util/internal.h"
#include "../sink.h"

namespace fs = std::filesystem;

namespace kb::log
{

class FileSink : public Sink
{
public:
    FileSink(const fs::path& filepath);
    ~FileSink() = default;
    void submit(const struct LogEntry &, const struct ChannelPresentation &) override;

private:
    fs::path filepath_;

    struct Internal;
    friend struct internal_deleter::Deleter<Internal>;
    internal_ptr<Internal> internal_ = nullptr;
};

} // namespace kb::log