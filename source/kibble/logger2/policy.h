#pragma once

namespace kb::log
{

/**
 * @brief Allows to modify and filter out log entries before dispatch
 *
 */
class Policy
{
public:
    virtual ~Policy() = default;

    /**
     * @brief Override this with custom behavior
     *
     * - The override can have side-effect on the entry to transform it.
     * - If it returns false, the entry will not be dispatched.
     *
     * @param entry
     * @return true
     * @return false
     */
    virtual bool transform_filter(struct LogEntry& entry) const = 0;
};

} // namespace kb::log