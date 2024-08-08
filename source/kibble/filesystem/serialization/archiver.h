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

template <typename T, template <typename> typename... Ps>
using satisfies_all = std::conjunction<Ps<T>...>;

template <typename T>
constexpr bool is_trivially_serializable_v =
    satisfies_all<T, std::is_standard_layout, std::is_trivially_copyable>::value;

} // namespace kb