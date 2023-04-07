#include "config/config.h"
#include "logger2/logger.h"
#include "string/string.h"
#include "toml++/toml.h"
#include <fmt/std.h>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace kb::cfg
{

template <typename NodeT>
void flatten(const NodeT& node, const std::string& name_chain, Settings::SettingsStorage& storage)
{
    hash_t name_hash = H_(name_chain.c_str());
    storage.key_names.insert({name_hash, name_chain});

    if constexpr (toml::is_string<decltype(node)>)
    {
        storage.scalars.insert({name_hash, Settings::SettingsScalar{*node}});
    }
    else if constexpr (toml::is_integer<decltype(node)>)
    {
        storage.scalars.insert({name_hash, Settings::SettingsScalar{*node}});
    }
    else if constexpr (toml::is_floating_point<decltype(node)>)
    {
        storage.scalars.insert({name_hash, Settings::SettingsScalar{*node}});
    }
    else if constexpr (toml::is_boolean<decltype(node)>)
    {
        storage.scalars.insert({name_hash, Settings::SettingsScalar{*node}});
    }
    else if constexpr (toml::is_table<decltype(node)>)
    {
        for (auto&& [k, val] : node)
        {
            auto key = k;
            val.visit([key, &name_chain, &storage](auto&& el) noexcept {
                std::string chain = name_chain + "." + key;
                flatten(el, chain, storage);
            });
        }
    }
    else if constexpr (toml::is_array<decltype(node)>)
    {
        size_t idx = 0;
        for (auto&& val : node)
        {
            val.visit([&name_chain, &idx, &storage](auto&& el) noexcept {
                std::string chain = name_chain + "[" + std::to_string(idx++) + "]";
                flatten(el, chain, storage);
            });
        }
        storage.arrays.insert({name_hash, {idx}});
    }
}

template <typename NodeT>
void serialize(NodeT& node, const std::string& name_chain, Settings::SettingsStorage& storage)
{
    hash_t name_hash = H_(name_chain.c_str());

    if constexpr (toml::is_string<decltype(node)>)
    {
        node = std::get<std::string>(storage.scalars.at(name_hash));
    }
    else if constexpr (toml::is_integer<decltype(node)>)
    {
        node = std::get<int64_t>(storage.scalars.at(name_hash));
    }
    else if constexpr (toml::is_floating_point<decltype(node)>)
    {
        node = std::get<double>(storage.scalars.at(name_hash));
    }
    else if constexpr (toml::is_boolean<decltype(node)>)
    {
        node = std::get<bool>(storage.scalars.at(name_hash));
    }
    else if constexpr (toml::is_table<decltype(node)>)
    {
        for (auto&& [k, val] : node)
        {
            auto key = k;
            val.visit([key, &name_chain, &storage](auto&& el) noexcept {
                std::string chain = name_chain + "." + key;
                serialize(el, chain, storage);
            });
        }
    }
    else if constexpr (toml::is_array<decltype(node)>)
    {
        size_t idx = 0;
        for (auto&& val : node)
        {
            val.visit([&name_chain, &idx, &storage](auto&& el) noexcept {
                std::string chain = name_chain + "[" + std::to_string(idx++) + "]";
                serialize(el, chain, storage);
            });
        }
        storage.arrays.insert({name_hash, {idx}});
    }
}

Settings::Settings(const kb::log::Channel* log_channel) : log_channel_(log_channel)
{
}

void Settings::load_toml(const fs::path& filepath, const std::string& _root_name)
{
    if (!fs::exists(filepath))
    {
        klog(log_channel_).uid("Settings").error("File does not exist:\n{}", filepath);
        return;
    }

    // Use file name as root name if no root name specified
    std::string root_name = _root_name;
    if (root_name.empty())
        root_name = filepath.stem().string();

    auto root_table = toml::parse_file(filepath.string());
    flatten(root_table, root_name, storage_);
}

void Settings::save_toml(const fs::path& filepath, const std::string& _root_name)
{
    if (!fs::exists(filepath))
    {
        klog(log_channel_).uid("Settings").error("File does not exist:\n{}", filepath);
        return;
    }

    // Use file name as root name if no root name specified
    std::string root_name = _root_name;
    if (root_name.empty())
        root_name = filepath.stem().string();

    auto root_table = toml::parse_file(filepath.string());
    serialize(root_table, root_name, storage_);

    std::ofstream ofs(filepath);
    ofs << root_table;
    ofs.close();
}

hash_t Settings::get_hash(hash_t hash, const std::string& def)
{
    return H_(get(hash, def));
}

hash_t Settings::get_hash_lower(hash_t hash, const std::string& def)
{
    std::string str(get(hash, def));
    su::to_lower(str);
    return H_(str);
}

hash_t Settings::get_hash_upper(hash_t hash, const std::string& def)
{
    std::string str(get(hash, def));
    su::to_upper(str);
    return H_(str);
}

void Settings::debug_dump() const
{
    klog(log_channel_).uid("Settings").verbose("-- DUMP --");
    for (auto&& [key, val] : storage_.scalars)
    {
        if (std::holds_alternative<int64_t>(val))
        {
            klog(log_channel_).raw().verbose("{}: {}", storage_.key_names.at(key), std::get<int64_t>(val));
        }
        else if (std::holds_alternative<double>(val))
        {
            klog(log_channel_).raw().verbose("{}: {}", storage_.key_names.at(key), std::get<double>(val));
        }
        else if (std::holds_alternative<bool>(val))
        {
            klog(log_channel_).raw().verbose("{}: {}", storage_.key_names.at(key), std::get<bool>(val));
        }
        else if (std::holds_alternative<std::string>(val))
        {
            klog(log_channel_).raw().verbose("{}: {}", storage_.key_names.at(key), std::get<std::string>(val));
        }
    }
}

} // namespace kb::cfg