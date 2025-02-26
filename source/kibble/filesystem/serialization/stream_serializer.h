#pragma once
#include <istream>
#include <ostream>

#include "kibble/filesystem/serialization/archiver.h"
#include "kibble/platform/types.h"

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
        if constexpr (Serializable<T>)
        {
            // For objects with a custom archiver (highest priority)
            return Archiver<T>::write(object, *this);
        }
        else if constexpr (is_trivially_serializable_v<T>)
        {
            // Only automatically serialize arithmetic and scoped enum types
            stream_.write(detail::opaque_cast(object), sizeof(T));
            return stream_.good();
        }
        else
        {
            // Require explicit serialization for any other type
            static_assert(Serializable<T> || is_trivially_serializable_v<T>,
                          "Type cannot be automatically serialized, define a custom Archiver<T> specialization.");
            return false;
        }
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
        if constexpr (Deserializable<T>)
        {
            // For objects with a custom archiver (highest priority)
            return Archiver<T>::read(object, *this);
        }
        else if constexpr (is_trivially_serializable_v<T>)
        {
            // Only automatically deserialize arithmetic and scoped enum types
            stream_.read(detail::opaque_cast(object), sizeof(T));
            return stream_.good();
        }
        else
        {
            // Require explicit serialization for any other type
            static_assert(Deserializable<T> || is_trivially_serializable_v<T>,
                          "Type cannot be automatically deserialized, define a custom Archiver<T> specialization.");
            return false;
        }
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