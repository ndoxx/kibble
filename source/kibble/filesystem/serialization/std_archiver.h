#pragma once
#include <array>
#include <filesystem>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "kibble/filesystem/serialization/stream_serializer.h"
#include "kibble/util/unordered_dense.h"

namespace kb
{

template <>
struct Archiver<std::string>
{
    static bool write(const std::string& object, StreamSerializer& ser);
    static bool read(std::string& object, StreamDeserializer& des);
};

template <>
struct Archiver<std::filesystem::path>
{
    static bool write(const std::filesystem::path& object, StreamSerializer& ser);
    static bool read(std::filesystem::path& object, StreamDeserializer& des);
};

template <typename T1, typename T2>
struct Archiver<std::pair<T1, T2>>
{
    static bool write(const std::pair<T1, T2>& object, StreamSerializer& ser)
    {
        return ser.write(object.first) && ser.write(object.second);
    }

    static bool read(std::pair<T1, T2>& object, StreamDeserializer& des)
    {
        return des.read(object.first) && des.read(object.second);
    }
};

template <typename... Types>
struct Archiver<std::tuple<Types...>>
{
    static bool write(const std::tuple<Types...>& object, StreamSerializer& ser)
    {
        return write_tuple(object, ser, std::index_sequence_for<Types...>{});
    }

    static bool read(std::tuple<Types...>& object, StreamDeserializer& des)
    {
        return read_tuple(object, des, std::index_sequence_for<Types...>{});
    }

private:
    template <std::size_t... Is>
    static bool write_tuple(const std::tuple<Types...>& object, StreamSerializer& ser, std::index_sequence<Is...>)
    {
        return (ser.write(std::get<Is>(object)) && ...);
    }

    template <std::size_t... Is>
    static bool read_tuple(std::tuple<Types...>& object, StreamDeserializer& des, std::index_sequence<Is...>)
    {
        return (des.read(std::get<Is>(object)) && ...);
    }
};

template <typename T>
struct Archiver<std::vector<T>>
{
    static bool write(const std::vector<T>& object, StreamSerializer& ser)
    {
        // Write the size of the vector
        uint64_t size = object.size();
        if (!ser.write(size))
        {
            return false;
        }

        if constexpr (is_trivially_serializable_v<T>)
        {
            return ser.write_blob(object.data(), object.size() * sizeof(T));
        }
        else
        {
            // Serialize each element
            for (const auto& item : object)
            {
                if (!ser.write(item))
                {
                    return false;
                }
            }
        }

        return true;
    }

    static bool read(std::vector<T>& object, StreamDeserializer& des)
    {
        // Read the size of the vector
        uint64_t size;
        if (!des.read(size))
        {
            return false;
        }
        object.resize(size);

        if constexpr (is_trivially_serializable_v<T>)
        {
            return des.read_blob(object.data(), object.size() * sizeof(T));
        }
        else
        {
            // Deserialize each element
            for (auto& item : object)
            {
                if (!des.read(item))
                {
                    return false;
                }
            }
        }

        return true;
    }
};

template <typename T, std::size_t N>
struct Archiver<std::array<T, N>>
{
    static bool write(const std::array<T, N>& object, StreamSerializer& ser)
    {
        if constexpr (is_trivially_serializable_v<T>)
        {
            return ser.write_blob(object.data(), object.size() * sizeof(T));
        }
        else
        {
            // Serialize each element
            for (const auto& item : object)
            {
                if (!ser.write(item))
                {
                    return false;
                }
            }
        }

        return true;
    }

    static bool read(std::array<T, N>& object, StreamDeserializer& des)
    {
        if constexpr (is_trivially_serializable_v<T>)
        {
            return des.read_blob(object.data(), object.size() * sizeof(T));
        }
        else
        {
            // Deserialize each element
            for (auto& item : object)
            {
                if (!des.read(item))
                {
                    return false;
                }
            }
        }

        return true;
    }
};

// Primary template for map-like containers
template <template <typename...> class Map, typename K, typename V, typename... Args>
struct MapArchiver
{
    static bool write(const Map<K, V, Args...>& object, StreamSerializer& ser)
    {
        // Write the size of the map
        uint64_t size = object.size();
        if (!ser.write(size))
        {
            return false;
        }

        // Serialize each key-value pair
        for (const auto& [key, value] : object)
        {
            if (!ser.write(key) || !ser.write(value))
            {
                return false;
            }
        }

        return true;
    }

    static bool read(Map<K, V, Args...>& object, StreamDeserializer& des)
    {
        // Read the size of the map
        uint64_t size;
        if (!des.read(size))
        {
            return false;
        }

        // Clear the existing map
        object.clear();

        // Reserve space for efficiency
        if constexpr (requires { object.reserve(size); })
        {
            object.reserve(size);
        }

        // Deserialize each key-value pair
        for (uint64_t ii = 0; ii < size; ++ii)
        {
            K key;
            V value;
            if (!des.read(key) || !des.read(value))
            {
                return false;
            }
            object.emplace(std::move(key), std::move(value));
        }

        return true;
    }
};

// Specialization for std::unordered_map
template <typename K, typename V, typename... Args>
struct Archiver<std::unordered_map<K, V, Args...>> : MapArchiver<std::unordered_map, K, V, Args...>
{
};

// Specialization for ankerl::unordered_dense::map
template <typename K, typename V, typename Hash, typename KeyEqual, typename AllocatorOrContainer, typename Bucket>
struct Archiver<ankerl::unordered_dense::map<K, V, Hash, KeyEqual, AllocatorOrContainer, Bucket>>
    : MapArchiver<ankerl::unordered_dense::map, K, V, Hash, KeyEqual, AllocatorOrContainer, Bucket>
{
};

// Primary template for set-like containers
template <template <typename...> class Set, typename T, typename... Args>
struct SetArchiver
{
    static bool write(const Set<T, Args...>& object, StreamSerializer& ser)
    {
        // Write the size of the set
        uint64_t size = object.size();
        if (!ser.write(size))
        {
            return false;
        }

        // Serialize each element
        for (const auto& item : object)
        {
            if (!ser.write(item))
            {
                return false;
            }
        }

        return true;
    }

    static bool read(Set<T, Args...>& object, StreamDeserializer& des)
    {
        // Read the size of the set
        uint64_t size;
        if (!des.read(size))
        {
            return false;
        }

        // Clear the existing set
        object.clear();

        // Reserve space for efficiency
        if constexpr (requires { object.reserve(size); })
        {
            object.reserve(size);
        }

        // Deserialize each element
        for (uint64_t ii = 0; ii < size; ++ii)
        {
            T item;
            if (!des.read(item))
            {
                return false;
            }
            object.insert(std::move(item));
        }

        return true;
    }
};

// Specialization for std::unordered_set
template <typename T, typename... Args>
struct Archiver<std::unordered_set<T, Args...>> : SetArchiver<std::unordered_set, T, Args...>
{
};

// Specialization for ankerl::unordered_dense::set
template <typename T, typename Hash, typename KeyEqual, typename AllocatorOrContainer>
struct Archiver<ankerl::unordered_dense::set<T, Hash, KeyEqual, AllocatorOrContainer>>
    : SetArchiver<ankerl::unordered_dense::set, T, Hash, KeyEqual, AllocatorOrContainer>
{
};

} // namespace kb