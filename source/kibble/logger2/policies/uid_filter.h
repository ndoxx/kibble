#pragma once

#include "../../hash/hash.h"
#include "../policy.h"
#include <set>

namespace kb::log
{

/**
 * @brief Allows to exclude all logs with UIDs that are NOT in the list
 *
 */
class UIDWhitelist : public Policy
{
public:
    UIDWhitelist() = default;
    ~UIDWhitelist() = default;

    /**
     * @brief Construct and initialize a whitelist policy
     *
     * @param enabled List of UID hashes to be put in the list
     */
    UIDWhitelist(std::set<hash_t> enabled);

    /**
     * @brief Add a uid to the list
     *
     * @param uid
     */
    inline void add(hash_t uid)
    {
        enabled_.insert(uid);
    }

    /**
     * @brief Remove a uid from the list
     *
     * @param uid
     */
    inline void remove(hash_t uid)
    {
        enabled_.erase(uid);
    }

    /**
     * @brief Check if the list contains a given uid
     *
     * @param uid
     * @return true
     * @return false
     */
    inline bool contains(hash_t uid) const
    {
        return enabled_.contains(uid);
    }

    /**
     * @brief Return true if the uid is either empty or in the list
     *
     * @param entry
     * @return true
     * @return false
     */
    bool transform_filter(struct LogEntry &entry) const override;

private:
    std::set<hash_t> enabled_;
};

/**
 * @brief Allows to exclude all logs with UIDs that are in the list
 *
 */
class UIDBlacklist : public Policy
{
public:
    UIDBlacklist() = default;
    ~UIDBlacklist() = default;

    /**
     * @brief Construct and initialize a blacklist policy
     *
     * @param enabled List of UID hashes to be put in the list
     */
    UIDBlacklist(std::set<hash_t> enabled);

    /**
     * @brief Add a uid to the list
     *
     * @param uid
     */
    inline void add(hash_t uid)
    {
        disabled_.insert(uid);
    }

    /**
     * @brief Remove a uid from the list
     *
     * @param uid
     */
    inline void remove(hash_t uid)
    {
        disabled_.erase(uid);
    }

    /**
     * @brief Check if the list contains a given uid
     *
     * @param uid
     * @return true
     * @return false
     */
    inline bool contains(hash_t uid) const
    {
        return disabled_.contains(uid);
    }

    /**
     * @brief Return true if the uid is either empty or NOT in the list
     *
     * @param entry
     * @return true
     * @return false
     */
    bool transform_filter(struct LogEntry &entry) const override;

private:
    std::set<hash_t> disabled_;
};

} // namespace kb::log