#include "kibble/logger/entry_builder.h"

namespace kb::log
{

EntryBuilder::EntryBuilder(const Channel& channel, int source_line, const char* source_file,
                           const char* source_function)
    : LogEntry{.source_location = {source_line, source_file, source_function},
               .timestamp = kb::TimeBase::timestamp(),
               .message = "",
               .uid_text = ""},
      channel_(&channel)
{
}

EntryBuilder::EntryBuilder(const Channel* channel, int source_line, const char* source_file,
                           const char* source_function)
    : LogEntry{.source_location = {source_line, source_file, source_function},
               .timestamp = kb::TimeBase::timestamp(),
               .message = "",
               .uid_text = ""},
      channel_(channel)
{
}

void EntryBuilder::log(std::string_view m)
{
    if (channel_)
    {
        message = m;
        channel_->submit(std::move(*this));
    }
}

void EntryBuilder::log(std::string&& m)
{
    if (channel_)
    {
        message = std::move(m);
        channel_->submit(std::move(*this));
    }
}

} // namespace kb::log