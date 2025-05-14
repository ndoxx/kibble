#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace kb
{

class StreamSerializer;
class StreamDeserializer;

/**
 * @brief Custom serializer/deserializer template for non-versioned serialization
 *
 * @details Provides a way to define custom serialization and deserialization
 * behavior for specific types. To use this functionality, specialize this template
 * for your type and define the write() and read() static methods.
 *
 * Example specialization:
 * @code
 * template <>
 * struct Archiver<MyClass>
 * {
 *     static bool write(const MyClass& object, StreamSerializer& ser)
 *     {
 *         // Write object data to serializer
 *         return ser.write(object.some_field) && ser.write(object.another_field);
 *     }
 *
 *     static bool read(MyClass& object, StreamDeserializer& des)
 *     {
 *         // Read object data from deserializer
 *         return des.read(object.some_field) && des.read(object.another_field);
 *     }
 * };
 * @endcode
 *
 * @tparam T The type for which to define custom serialization
 * @see Serializable
 * @see Deserializable
 */
template <typename T>
struct Archiver
{
    // static bool write(const T& object, StreamSerializer& ser);
    // static bool read(T& object, StreamDeserializer& des);
};

/**
 * @brief Custom versioned serializer/deserializer template
 *
 * @details Provides a way to define version-aware serialization and deserialization
 * behavior for specific types. This enables backward compatibility when the structure
 * of serialized data changes over time.
 *
 * To use this functionality, specialize this template for your type and define:
 * - k_current_version: the current version of the serialization format
 * - write(): method to write object data with version awareness
 * - read(): method to read object data with version awareness
 *
 * Example specialization:
 * @code
 * template <>
 * struct VersionedArchiver<MyClass>
 * {
 *     static constexpr uint32_t k_current_version = 2;
 *
 *     static bool write(const MyClass& object, StreamSerializer& ser, uint32_t version)
 *     {
 *         // Always write current data
 *         if (!ser.write(object.new_field) || !ser.write(object.common_field))
 *             return false;
 *         return true;
 *     }
 *
 *     static bool read(MyClass& object, StreamDeserializer& des, uint32_t version)
 *     {
 *         // Handle different versions
 *         if (version >= 2)
 *         {
 *             // Read format version 2+
 *             if (!des.read(object.new_field) || !des.read(object.common_field))
 *                 return false;
 *         }
 *         else
 *         {
 *             // Read legacy format (version 1)
 *             if (!des.read(object.common_field))
 *                 return false;
 *             // Set default value for fields not present in older versions
 *             object.new_field = 0;
 *         }
 *         return true;
 *     }
 * };
 * @endcode
 *
 * @tparam T The type for which to define versioned serialization
 * @see VersionedSerializable
 * @see VersionedDeserializable
 */
template <typename T>
struct VersionedArchiver
{
    // static constexpr uint32_t k_current_version = 1;
    // static bool write(const T& object, StreamSerializer& ser, uint32_t version);
    // static bool read(T& object, StreamDeserializer& des, uint32_t version);
};

/**
 * @brief Concept that defines requirements for serializable types
 *
 * @details A type T satisfies this concept if:
 * - It has a specialization of Archiver<T>
 * - That specialization defines a static write() method with the correct signature
 *
 * @tparam T The type to check for serializability
 */
template <typename T>
concept Serializable = requires(const T& object, StreamSerializer& ser) {
    { Archiver<T>::write(object, ser) } -> std::same_as<bool>;
};

/**
 * @brief Concept that defines requirements for deserializable types
 *
 * @details A type T satisfies this concept if:
 * - It has a specialization of Archiver<T>
 * - That specialization defines a static read() method with the correct signature
 *
 * @tparam T The type to check for deserializability
 */
template <typename T>
concept Deserializable = requires(T& object, StreamDeserializer& des) {
    { Archiver<T>::read(object, des) } -> std::same_as<bool>;
};

/**
 * @brief Concept that defines requirements for version-aware serializable types
 *
 * @details A type T satisfies this concept if:
 * - It has a specialization of VersionedArchiver<T>
 * - That specialization defines a k_current_version constant
 * - That specialization defines a static write() method with the correct signature
 *   including a version parameter
 *
 * @tparam T The type to check for versioned serializability
 */
template <typename T>
concept VersionedSerializable = requires(const T& object, StreamSerializer& ser, uint32_t version) {
    { VersionedArchiver<T>::k_current_version } -> std::convertible_to<uint32_t>;
    { VersionedArchiver<T>::write(object, ser, version) } -> std::same_as<bool>;
};

/**
 * @brief Concept that defines requirements for version-aware deserializable types
 *
 * @details A type T satisfies this concept if:
 * - It has a specialization of VersionedArchiver<T>
 * - That specialization defines a k_current_version constant
 * - That specialization defines a static read() method with the correct signature
 *   including a version parameter
 *
 * @tparam T The type to check for versioned deserializability
 */
template <typename T>
concept VersionedDeserializable = requires(T& object, StreamDeserializer& des, uint32_t version) {
    { VersionedArchiver<T>::k_current_version } -> std::convertible_to<uint32_t>;
    { VersionedArchiver<T>::read(object, des, version) } -> std::same_as<bool>;
};

/**
 * @brief Type trait that identifies types that can be trivially serialized
 *
 * @details A type is considered trivially serializable if it's either:
 * - An arithmetic type (int, float, etc.)
 * - A scoped enumeration (enum class)
 *
 * These types can be serialized directly by copying their binary representation.
 *
 * @tparam T The type to check
 */
template <class T>
struct is_trivially_serializable
    : std::integral_constant<bool, std::is_arithmetic<T>::value || std::is_scoped_enum<T>::value>
{
};

/**
 * @brief Helper variable template for is_trivially_serializable
 *
 * @tparam T The type to check
 */
template <class T>
constexpr bool is_trivially_serializable_v = is_trivially_serializable<T>::value;

} // namespace kb