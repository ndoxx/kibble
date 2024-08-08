#pragma once
#include <istream>
#include <ostream>

#include "archiver.h"

namespace kb
{

namespace detail
{
template <typename T, typename = std::enable_if_t<std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>>>
inline char* opaque_cast(T& in)
{
    return reinterpret_cast<char*>(&in);
}

template <typename T, typename = std::enable_if_t<std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>>>
inline const char* opaque_cast(const T& in)
{
    return reinterpret_cast<const char*>(&in);
}
} // namespace detail

/**
 * @brief Basic generic stream serializer
 *
 * Known limitations:
 *     - Endianness conversion not supported
 *
 */
class StreamSerializer
{
public:
    StreamSerializer(std::ostream& stream);

    template <typename T>
    inline bool write(const T& object)
    {
        // For objects with a custom archiver (higher priority)
        if constexpr (Serializable<T>)
        {
            return Archiver<T>::write(object, *this);
        }
        // Default serializer for POD data
        else if constexpr (is_trivially_serializable_v<T>)
        {
            stream_.write(reinterpret_cast<const char*>(&object), sizeof(T));
            return stream_.good();
        }
        else
        {
            static_assert(false, "Type cannot be serialized");
        }

        return false;
    }

    inline bool write_blob(const void* buffer, size_t size)
    {
        stream_.write(reinterpret_cast<const char*>(buffer), ssize_t(size));
        return stream_.good();
    }

    inline bool good() const
    {
        return stream_.good();
    }

    inline void seek(ssize_t pos)
    {
        stream_.seekp(pos);
    }

    inline ssize_t tell() const
    {
        return stream_.tellp();
    }

private:
    std::ostream& stream_;
};

/**
 * @brief Basic generic stream deserializer
 *
 * Known limitations:
 *     - Endianness conversion not supported
 *
 */
class StreamDeserializer
{
public:
    StreamDeserializer(std::istream& stream);

    template <typename T>
    inline bool read(T& object)
    {
        // For objects with a custom archiver (higher priority)
        if constexpr (Deserializable<T>)
        {
            return Archiver<T>::read(object, *this);
        }
        // Default deserializer for POD data
        else if constexpr (is_trivially_serializable_v<T>)
        {
            stream_.read(reinterpret_cast<char*>(&object), sizeof(T));
            return stream_.good();
        }
        else
        {
            static_assert(false, "Type cannot be deserialized");
        }

        return false;
    }

    inline bool read_blob(void* buffer, size_t size)
    {
        stream_.read(reinterpret_cast<char*>(buffer), ssize_t(size));
        return stream_.good();
    }

    template <typename T>
    inline T read()
    {
        T object;
        if (!read(object))
        {
            throw std::runtime_error("Error reading object");
        }
        return object;
    }

    inline bool good() const
    {
        return stream_.good();
    }

    inline void seek(ssize_t pos)
    {
        stream_.seekg(pos);
    }

    inline ssize_t tell() const
    {
        return stream_.tellg();
    }

private:
    std::istream& stream_;
};

} // namespace kb