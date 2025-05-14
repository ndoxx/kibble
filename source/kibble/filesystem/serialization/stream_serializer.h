#pragma once
#include <istream>
#include <ostream>

#include "kibble/filesystem/serialization/archiver.h"
#include "kibble/platform/types.h"

namespace kb
{

/**
 * @brief Basic generic stream serializer
 *
 * @details This class provides functionality to serialize objects to an output stream.
 * The serialization behavior depends on the type of object:
 * - Types with a specialized VersionedArchiver will use versioned serialization
 * - Types with a specialized Archiver will use non-versioned serialization
 * - Trivially serializable types (arithmetic types and scoped enums) are automatically serialized
 *
 * When using the versioned serialization, a version tag and version number are written
 * before the actual data, allowing for backward compatibility.
 *
 * @note Known limitations:
 *     - Endianness conversion not supported
 *
 * @see VersionedArchiver
 * @see Archiver
 */
class StreamSerializer
{
public:
    /**
     * @brief Constructs a serializer that writes to the given output stream
     *
     * @param stream Reference to the output stream where data will be written
     */
    StreamSerializer(std::ostream& stream);

    /**
     * @brief Serializes an object to the output stream
     *
     * @details The serialization process follows these priorities:
     * 1. If T has a VersionedArchiver specialization, use that with version information
     * 2. If T has an Archiver specialization, use that
     * 3. If T is an arithmetic type or scoped enum, serialize directly
     * 4. Otherwise, fail at compile time
     *
     * @tparam T Type of object to serialize
     * @param object The object to serialize
     * @return true if serialization was successful
     * @return false if any error occurred during serialization
     */
    template <typename T>
    inline bool write(const T& object)
    {
        // For objects with a custom versioned archiver (highest priority)
        if constexpr (VersionedSerializable<T>)
        {
            // First, write (tagged) version info
            const uint32_t version = VersionedArchiver<T>::k_current_version;
            if (!write_version(version))
            {
                return false;
            }
            return VersionedArchiver<T>::write(object, *this, version);
        }
        // For objects with a custom archiver (version = 0, legacy)
        else if constexpr (Serializable<T>)
        {
            return Archiver<T>::write(object, *this);
        }
        // Only automatically serialize arithmetic and scoped enum types
        else if constexpr (is_trivially_serializable_v<T>)
        {
            stream_.write(reinterpret_cast<const char*>(&object), sizeof(T));
            return good();
        }
        // Require explicit serialization for any other type
        else
        {
            static_assert(VersionedSerializable<T> || Serializable<T> || is_trivially_serializable_v<T>,
                          "Type cannot be automatically serialized, define a custom Archiver<T> specialization.");
            return false;
        }
    }

    /**
     * @brief Writes a raw binary blob to the output stream
     *
     * @param buffer Pointer to the data to write
     * @param size Number of bytes to write
     * @return true if writing was successful
     * @return false if any error occurred
     */
    inline bool write_blob(const void* buffer, size_t size)
    {
        stream_.write(reinterpret_cast<const char*>(buffer), ssize_t(size));
        return good();
    }

    /**
     * @brief Checks if the output stream is in a good state
     *
     * @return true if the stream is good
     * @return false if the stream has encountered an error
     */
    inline bool good() const
    {
        return stream_.good();
    }

    /**
     * @brief Sets the position of the write pointer in the output stream
     *
     * @param pos The position to seek to
     */
    inline void seek(ssize_t pos)
    {
        stream_.seekp(pos);
    }

    /**
     * @brief Gets the current position of the write pointer in the output stream
     *
     * @return Current write position
     */
    inline ssize_t tell() const
    {
        return stream_.tellp();
    }

private:
    /**
     * @internal
     * @brief Writes version information to the stream
     *
     * @details Writes a version tag followed by the version number.
     * The tag helps identify versioned data during deserialization.
     *
     * @param version The version number to write
     * @return true if writing was successful
     * @return false if any error occurred
     */
    bool write_version(uint32_t version);

private:
    std::ostream& stream_; ///< Reference to the output stream
};

/**
 * @brief Basic generic stream deserializer
 *
 * @details This class provides functionality to deserialize objects from an input stream.
 * The deserialization behavior depends on the type of object:
 * - Types with a specialized VersionedArchiver will use versioned deserialization
 * - Types with a specialized Archiver will use non-versioned deserialization
 * - Trivially serializable types (arithmetic types and scoped enums) are automatically deserialized
 *
 * When using versioned deserialization, the deserializer attempts to read a version tag and number.
 * If found, the appropriate version-specific deserialization is used, enabling backward compatibility.
 *
 * @note Known limitations:
 *     - Endianness conversion not supported
 *
 * @see VersionedArchiver
 * @see Archiver
 */
class StreamDeserializer
{
public:
    /**
     * @brief Constructs a deserializer that reads from the given input stream
     *
     * @param stream Reference to the input stream from which data will be read
     */
    StreamDeserializer(std::istream& stream);

    /**
     * @brief Deserializes an object from the input stream
     *
     * @details The deserialization process follows these priorities:
     * 1. If T has a VersionedArchiver specialization, use that with version information
     * 2. If T has an Archiver specialization, use that
     * 3. If T is an arithmetic type or scoped enum, deserialize directly
     * 4. Otherwise, fail at compile time
     *
     * @tparam T Type of object to deserialize
     * @param object The object where deserialized data will be stored
     * @return true if deserialization was successful
     * @return false if any error occurred during deserialization
     */
    template <typename T>
    inline bool read(T& object)
    {
        // For objects with a custom versioned archiver (highest priority)
        if constexpr (VersionedDeserializable<T>)
        {
            // First, try to read (tagged) version info
            uint32_t version;
            if (!read_version(version))
            {
                return false;
            }
            // Call the versioned archiver
            return VersionedArchiver<T>::read(object, *this, version);
        }
        // For objects with a custom archiver (version = 0, legacy)
        else if constexpr (Deserializable<T>)
        {
            return Archiver<T>::read(object, *this);
        }
        // Only automatically deserialize arithmetic and scoped enum types
        else if constexpr (is_trivially_serializable_v<T>)
        {
            stream_.read(reinterpret_cast<char*>(&object), sizeof(T));
            return good();
        }
        // Require explicit serialization for any other type
        else
        {
            static_assert(VersionedDeserializable<T> || Deserializable<T> || is_trivially_serializable_v<T>,
                          "Type cannot be automatically deserialized, define a custom Archiver<T> specialization.");
            return false;
        }
    }

    /**
     * @brief Reads a raw binary blob from the input stream
     *
     * @param buffer Pointer to the memory where data will be stored
     * @param size Number of bytes to read
     * @return true if reading was successful
     * @return false if any error occurred
     */
    inline bool read_blob(void* buffer, size_t size)
    {
        stream_.read(reinterpret_cast<char*>(buffer), ssize_t(size));
        return good();
    }

    /**
     * @brief Reads an object from the input stream and returns it
     *
     * @details This is a convenience wrapper around the read() method that throws an exception
     * if reading fails, instead of returning a boolean status.
     *
     * @tparam T Type of object to deserialize
     * @return The deserialized object
     * @throws std::runtime_error if deserialization fails
     */
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

    /**
     * @brief Checks if the input stream is in a good state
     *
     * @return true if the stream is good
     * @return false if the stream has encountered an error
     */
    inline bool good() const
    {
        return stream_.good();
    }

    /**
     * @brief Sets the position of the read pointer in the input stream
     *
     * @param pos The position to seek to
     */
    inline void seek(ssize_t pos)
    {
        stream_.seekg(pos);
    }

    /**
     * @brief Gets the current position of the read pointer in the input stream
     *
     * @return Current read position
     */
    inline ssize_t tell() const
    {
        return stream_.tellg();
    }

private:
    /**
     * @internal
     * @brief Reads version information from the stream
     *
     * @details Attempts to read a version tag followed by a version number.
     * If the tag is found, the version number is read. If not, the stream position
     * is reset and version is set to 0 (legacy format).
     *
     * @param version [out] The version number read from the stream, or 0 if no version tag was found
     * @return true if reading was successful or if version tag wasn't found (legacy format)
     * @return false if any error occurred during reading
     */
    bool read_version(uint32_t& version);

private:
    std::istream& stream_; ///< Reference to the input stream
};

} // namespace kb