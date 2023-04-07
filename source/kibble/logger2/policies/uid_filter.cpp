#include "uid_filter.h"
#include "../entry.h"

namespace kb::log
{

UIDWhitelist::UIDWhitelist(std::set<hash_t> enabled) : enabled_(enabled)
{
}

bool UIDWhitelist::transform_filter(LogEntry& entry) const
{
    return entry.uid_text.empty() || int8_t(entry.severity) <= 2 || contains(H_(entry.uid_text));
}

UIDBlacklist::UIDBlacklist(std::set<hash_t> disabled) : disabled_(disabled)
{
}

bool UIDBlacklist::transform_filter(LogEntry& entry) const
{
    return entry.uid_text.empty() || int8_t(entry.severity) <= 2 || !contains(H_(entry.uid_text));
}

} // namespace kb::log