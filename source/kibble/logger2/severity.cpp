#include "severity.h"
#include "entry.h"

namespace kb::log
{

DefaultSeverityLevelPolicy::DefaultSeverityLevelPolicy(Severity level) : level_{level}
{
}

bool DefaultSeverityLevelPolicy::filter(const struct LogEntry &entry) const
{
    return entry.severity <= level_;
}

} // namespace kb::log