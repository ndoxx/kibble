#pragma once

#include <array>
#include <initializer_list>

namespace kb
{

/**
 * @brief Fixed size array constructible with an std::initializer_list.
 * This is a specialization of std::array.
 * See: https://stackoverflow.com/questions/6893700/how-to-construct-stdarray-object-with-initializer-list
 *
 * @tparam T element type
 * @tparam size array size
 * @tparam EnumT index type
 */
template <typename T, std::size_t size, typename EnumT = std::size_t>
struct addressable_array : public std::array<T, size>
{
    typedef std::array<T, size> base_t;
    typedef typename base_t::reference reference;
    typedef typename base_t::const_reference const_reference;
    typedef typename base_t::size_type size_type;

    /**
     * @brief Perfect forward the arguments to the base constructor.
     *
     * @tparam E Element type
     * @param e arguments in the list
     */
    template <typename... E>
    addressable_array(E&&... e) : base_t{{std::forward<E>(e)...}}
    {
    }

    /**
     * @brief non-const indexed access operator.
     *
     * @param n index
     * @return reference to the element at index n
     */
    reference operator[](EnumT n)
    {
        return base_t::operator[](static_cast<size_type>(n));
    }

    /**
     * @brief const indexed access operator.
     *
     * @param n index
     * @return const reference to the element at index n
     */
    const_reference operator[](EnumT n) const
    {
        return base_t::operator[](static_cast<size_type>(n));
    }
};

} // namespace kb