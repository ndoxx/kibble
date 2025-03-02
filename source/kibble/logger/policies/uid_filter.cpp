#include "kibble/logger/policies/uid_filter.h"

#include <utility>

#include "kibble/logger/entry.h"
#include <utility>

namespace kb::log
{

UIDWhitelist::UIDWhitelist(ankerl::unordered_dense::set<hash_t> enabled) : enabled_(std::move(enabled))
{
}

bool UIDWhitelist::transform_filter(LogEntry& entry) const
{
    return entry.uid_text.empty() || int8_t(entry.severity) <= 2 || contains(H_(entry.uid_text));
}

UIDBlacklist::UIDBlacklist(ankerl::unordered_dense::set<hash_t> disabled) : disabled_(std::move(disabled))
{
}

bool UIDBlacklist::transform_filter(LogEntry& entry) const
{
    return entry.uid_text.empty() || int8_t(entry.severity) <= 2 || !contains(H_(entry.uid_text));
}

} // namespace kb::log