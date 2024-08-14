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
        size_t size = object.size();
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

    static bool read(std::vector<T>& object, StreamDeserializer& des)
    {
        // Read the size of the vector
        size_t size;
        if (!des.read(size))
        {
            return false;
        }
        object.resize(size);

        // Deserialize each element
        for (auto& item : object)
        {
            if (!des.read(item))
            {
                return false;
            }
        }

        return true;
    }
};

template <typename T, std::size_t N>
struct Archiver<std::array<T, N>>
{
    /*
        NOTE(ndx): Enable this specialization only for non-trivially serializable types.
        This avoids an infinite recursion loop, because std::array<T,N> is trivially serializable
        if T is trivially serializable
    */
    template <typename U = T>
    static constexpr bool enable_specialization = !is_trivially_serializable_v<std::array<U, N>>;
};

// Specialization for arrays of non-trivially serializable types
template <typename T, std::size_t N>
    requires(!is_trivially_serializable_v<T>)
struct Archiver<std::array<T, N>>
{
    static bool write(const std::array<T, N>& object, StreamSerializer& ser)
    {
        for (const auto& item : object)
        {
            if (!ser.write(item))
            {
                return false;
            }
        }

        return true;
    }

    static bool read(std::array<T, N>& object, StreamDeserializer& des)
    {
        for (auto& item : object)
        {
            if (!des.read(item))
            {
                return false;
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
        size_t size = object.size();
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
        size_t size;
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
        for (size_t i = 0; i < size; ++i)
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
        size_t size = object.size();
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
        size_t size;
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
        for (size_t i = 0; i < size; ++i)
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