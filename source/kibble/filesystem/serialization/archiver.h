#pragma once

#include <concepts>
#include <type_traits>

namespace kb
{

class StreamSerializer;
class StreamDeserializer;

/**
 * @brief Custom serializer / deserializer
 *
 * Just define write() and read() functions to specify a custom
 * (de)serialization behavior.
 *
 * @tparam T
 */
template <typename T>
struct Archiver
{
    // static bool write(const T&, StreamSerializer&);
    // static bool read(T&, StreamDeserializer&);
};

template <typename T>
concept Serializable = requires(const T& object, StreamSerializer& ser) {
    { Archiver<T>::write(object, ser) } -> std::same_as<bool>;
};

template <typename T>
concept Deserializable = requires(T& object, StreamDeserializer& des) {
    { Archiver<T>::read(object, des) } -> std::same_as<bool>;
};

template <class T>
struct is_trivially_serializable
    : std::integral_constant<bool, std::is_arithmetic<T>::value || std::is_scoped_enum<T>::value>
{
};

template <class T>
constexpr bool is_trivially_serializable_v = is_trivially_serializable<T>::value;

} // namespace kb