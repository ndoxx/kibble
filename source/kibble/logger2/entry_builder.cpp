#include "entry_builder.h"

namespace kb::log
{

EntryBuilder::EntryBuilder(Channel &channel, int source_line, const char *source_file, const char *source_function)
    : LogEntry{.source_location = {source_line, source_file, source_function}, .timestamp = kb::TimeBase::timestamp()},
      channel_(channel)
{
}

void EntryBuilder::log(Severity s, std::string_view m)
{
    severity = s;
    message = m;
    channel_.submit(*this);
}

} // namespace kb::log