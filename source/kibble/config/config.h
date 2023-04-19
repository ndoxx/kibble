#pragma once

#include "../hash/hash.h"
#include <filesystem>
#include <unordered_map>
#include <memory>
#include <variant>

namespace fs = std::filesystem;

namespace kb::log
{
class Channel;
}

namespace kb::cfg
{

/**
 * @brief  The Settings class can aggregate key/value pairs obtained from multiple TOML files.
 * A key is a string hash that reflects the value hierarchy in the file it was pulled from.
 * Say "client.toml" is loaded, then the root name will be "client" (by default).
 * Then, to access a property, one should use the get/set functions with a hash of this form:
 *     "client.window.width"_h
 * If an array was parsed, the bracket notation is used:
 *     "erwin.logger.channels[2].name"_h
 * The string util su::h_concat can facilitate the computation of such hashes.
 * Array sizes can be queried with get_array_size().
 * If a value in the Settings instance has been modified and you want to serialize the changes,
 * the save_toml() function is used WITH THE SAME arguments used in load_toml(), and all
 * modifications relative to this file will be written out.
 *
 */
class Settings
{
public:
    Settings(const kb::log::Channel* log_channel = nullptr);

    /**
     * @brief Parse a TOML file and add the new properties to this object
     *
     * @param filepath Path to the config file
     * @param root_name Name of the root element. If left empty, the root name will
     * be set to the file name stem.
     */
    void load_toml(const fs::path& filepath, const std::string& root_name = "");

    /**
     * @brief Save all properties inherited from a TOML file back to the file
     *
     * @param filepath Path to the config file
     * @param root_name Name of the root element. Must match the root name used
     * when the file was loaded.
     */
    void save_toml(const fs::path& filepath, const std::string& root_name = "");

    /**
     * @brief Clear all properties
     */
    inline void clear()
    {
        storage_.clear();
    }

    /**
     * @brief Get the value of a property registered in this object
     *
     * @tparam T Underlying type of the property
     * @param hash Hash name of the property
     * @param default_value Default value to return in case the property was not set
     * @return T The value
     */
    template <typename T>
    T get(hash_t hash, const T& default_value) const;

    /**
     * @brief Set the value of an existing property
     *
     * @tparam T Underlying type of the property
     * @param hash Hash name of the property
     * @param value The value to set the property to
     * @return true if the property was found and successfully modified
     * @return false otherwise
     */
    template <typename T>
    bool set(hash_t hash, const T& value);

    /**
     * @brief Get the hash of a string property
     *
     * @param hash Hash name of the property
     * @param def Default string value to use in case the property was not set
     * @return hash_t The hash
     */
    hash_t get_hash(hash_t hash, const std::string& def);

    /**
     * @brief Find a string property, convert it to lower case then return its hash
     *
     * @param hash Hash name of the property
     * @param def Default string value to use in case the property was not set
     * @return hash_t The hash
     */
    hash_t get_hash_lower(hash_t hash, const std::string& def);

    /**
     * @brief Find a string property, convert it to upper case then return its hash
     *
     * @param hash Hash name of the property
     * @param def Default string value to use in case the property was not set
     * @return hash_t The hash
     */
    hash_t get_hash_upper(hash_t hash, const std::string& def);

    /**
     * @brief Get a boolean property at that name
     *
     * @param hash Hash name of the property
     * @return true if the property was set to true
     * @return false if the property was set to false or was not set in the first place
     */
    inline bool is(hash_t hash) const
    {
        return get<bool>(hash, false);
    }

    /**
     * @brief Check if an arry property exists at that name
     *
     * @param hash Hash name of the property
     * @return true if an arry was set at that name
     * @return false otherwise
     */
    inline bool has_array(hash_t hash) const
    {
        return storage_.arrays.find(hash) != storage_.arrays.end();
    }

    /**
     * @brief Get the size of the array set at that name
     *
     * @param hash Hash name of the property
     * @return size_t The array size
     */
    inline size_t get_array_size(hash_t hash) const
    {
        auto findit = storage_.arrays.find(hash);
        if (findit != storage_.arrays.end())
            return findit->second.size;
        return 0;
    }

    void debug_dump() const;

    using SettingsScalar = std::variant<int64_t, double, bool, std::string>;

private:
    struct ArrayDescriptor
    {
        size_t size;
    };

    struct SettingsStorage
    {
        std::unordered_map<hash_t, SettingsScalar> scalars;
        std::unordered_map<hash_t, ArrayDescriptor> arrays;
        std::unordered_map<hash_t, std::string> key_names;

        void clear()
        {
            scalars.clear();
            arrays.clear();
            key_names.clear();
        }
    };

public:
    template <typename NodeT>
    friend void flatten(const NodeT&, const std::string&, SettingsStorage&);
    template <typename NodeT>
    friend void serialize(NodeT&, const std::string&, Settings::SettingsStorage&);

private:
    SettingsStorage storage_;
    const kb::log::Channel* log_channel_ = nullptr;
};

template <typename T>
T Settings::get(hash_t hash, const T& default_value) const
{
    auto findit = storage_.scalars.find(hash);
    if (findit != storage_.scalars.end())
    {
        const auto& var = findit->second;
        if (std::holds_alternative<T>(var))
            return std::get<T>(var);
    }
    return default_value;
}

template <typename T>
bool Settings::set(hash_t hash, const T& value)
{
    auto findit = storage_.scalars.find(hash);
    if (findit != storage_.scalars.end())
    {
        auto& var = findit->second;
        if (std::holds_alternative<T>(var))
        {
            var = value;
            return true;
        }
    }
    return false;
}

template <>
inline size_t Settings::get<size_t>(hash_t hash, const size_t& default_value) const
{
    return size_t(get<int64_t>(hash, int64_t(default_value)));
}

template <>
inline uint32_t Settings::get<uint32_t>(hash_t hash, const uint32_t& default_value) const
{
    return uint32_t(get<int64_t>(hash, int64_t(default_value)));
}

template <>
inline int32_t Settings::get<int32_t>(hash_t hash, const int32_t& default_value) const
{
    return int32_t(get<int64_t>(hash, int64_t(default_value)));
}

template <>
inline float Settings::get<float>(hash_t hash, const float& default_value) const
{
    return float(get<double>(hash, double(default_value)));
}

template <>
inline bool Settings::set<size_t>(hash_t hash, const size_t& value)
{
    return set<int64_t>(hash, int64_t(value));
}

template <>
inline bool Settings::set<uint32_t>(hash_t hash, const uint32_t& value)
{
    return set<int64_t>(hash, int64_t(value));
}

template <>
inline bool Settings::set<int32_t>(hash_t hash, const int32_t& value)
{
    return set<int64_t>(hash, int64_t(value));
}

template <>
inline bool Settings::set<float>(hash_t hash, const float& value)
{
    return set<double>(hash, double(value));
}

} // namespace kb::cfg