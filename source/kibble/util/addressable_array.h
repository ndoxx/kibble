#pragma once

#include <array>
#include <initializer_list>

namespace kb
{

/*
	Fixed size array constructible with an initializer_list. From here:
	https://stackoverflow.com/questions/6893700/how-to-construct-stdarray-object-with-initializer-list
*/
template <typename T, std::size_t size, typename EnumT=std::size_t>
struct addressable_array : public std::array<T, size>
{
    typedef std::array<T, size> base_t;
    typedef typename base_t::reference reference;
    typedef typename base_t::const_reference const_reference;
    typedef typename base_t::size_type size_type;

    template<typename ...E>
	addressable_array(E&&...e): base_t{{std::forward<E>(e)...}} {}

    reference operator[](EnumT n)
    {
        return base_t::operator[](static_cast<size_type>(n));
    }

    const_reference operator[](EnumT n) const
    {
        return base_t::operator[](static_cast<size_type>(n));
    }
};


} // namespace kb