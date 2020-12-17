#pragma once
/*
 * The Settings class can aggregate key/value pairs obtained from multiple TOML files.
 * A key is a string hash that reflects the value hierarchy in the file it was pulled from.
 * Say "client.toml" is loaded, then the first node name will be "client" (by default).
 * Then, to access a property, one should use the get/set functions with a hash of this form:
 *     "client.window.width"_h
 * If an array was parsed, the bracket notation is used:
 *     "erwin.logger.channels[2].name"_h
 * The string util su::h_concat can facilitate the computation of such hashes.
 * Array sizes can be queried with get_array_size().
 * If a value in the Settings instance has been modified and you want to serialize the changes,
 * the save_toml() function is used WITH THE SAME arguments used in load_toml(), and all
 * modifications relative to this file will be written out.
 */

#include "../hash/hash.h"
#include <filesystem>
#include <map>
#include <memory>
#include <variant>

namespace fs = std::filesystem;
namespace kb
{
namespace cfg
{

using SettingsScalar = std::variant<int64_t, double, bool, std::string>;

struct ArrayDescriptor
{
    size_t size;
};

struct SettingsStorage
{
    std::map<hash_t, SettingsScalar> scalars;
    std::map<hash_t, ArrayDescriptor> arrays;
#ifdef K_DEBUG
    std::map<hash_t, std::string> key_names;
#endif
};

class Settings
{
public:
    void load_toml(const fs::path& filepath, const std::string& root_name = "");
    void save_toml(const fs::path& filepath, const std::string& root_name = "");

    template <typename T> T get(hash_t hash, const T& default_value) const;
    template <typename T> bool set(hash_t hash, const T& value);
    hash_t get_hash(hash_t hash, const std::string& def);
    hash_t get_hash_lower(hash_t hash, const std::string& def);
    hash_t get_hash_upper(hash_t hash, const std::string& def);
    inline bool is(hash_t hash) const { return get<bool>(hash, false); }
    inline bool has_array(hash_t hash) const { return storage_.arrays.find(hash) != storage_.arrays.end(); }
    inline size_t get_array_size(hash_t hash) const
    {
        auto findit = storage_.arrays.find(hash);
        if(findit != storage_.arrays.end())
            return findit->second.size;
        return 0;
    }

#ifdef K_DEBUG
    void debug_dump() const;
#endif

private:
    SettingsStorage storage_;
};

template <typename T> T Settings::get(hash_t hash, const T& default_value) const
{
    auto findit = storage_.scalars.find(hash);
    if(findit != storage_.scalars.end())
    {
        const auto& var = findit->second;
        if(std::holds_alternative<T>(var))
            return std::get<T>(var);
    }
    return default_value;
}

template <typename T> bool Settings::set(hash_t hash, const T& value)
{
    auto findit = storage_.scalars.find(hash);
    if(findit != storage_.scalars.end())
    {
        auto& var = findit->second;
        if(std::holds_alternative<T>(var))
        {
            var = value;
            return true;
        }
    }
    return false;
}

template <> inline size_t Settings::get<size_t>(hash_t hash, const size_t& default_value) const
{
    return size_t(get<int64_t>(hash, int64_t(default_value)));
}

template <> inline uint32_t Settings::get<uint32_t>(hash_t hash, const uint32_t& default_value) const
{
    return uint32_t(get<int64_t>(hash, int64_t(default_value)));
}

template <> inline int32_t Settings::get<int32_t>(hash_t hash, const int32_t& default_value) const
{
    return int32_t(get<int64_t>(hash, int64_t(default_value)));
}

template <> inline float Settings::get<float>(hash_t hash, const float& default_value) const
{
    return float(get<double>(hash, double(default_value)));
}

template <> inline bool Settings::set<size_t>(hash_t hash, const size_t& value)
{
    return set<int64_t>(hash, int64_t(value));
}

template <> inline bool Settings::set<uint32_t>(hash_t hash, const uint32_t& value)
{
    return set<int64_t>(hash, int64_t(value));
}

template <> inline bool Settings::set<int32_t>(hash_t hash, const int32_t& value)
{
    return set<int64_t>(hash, int64_t(value));
}

template <> inline bool Settings::set<float>(hash_t hash, const float& value)
{
    return set<double>(hash, double(value));
}

} // namespace cfg
} // namespace kb