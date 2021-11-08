/**
 * @file ct_counter.h
 * @brief Compile-time counter utility for automatic ID generation.
 * Adapted from: https://stackoverflow.com/questions/6166337/does-c-support-compile-time-counters \n
 * Mentioned by Rez Bot in: https://www.youtube.com/watch?v=WbwXxms80Z4 \n
 * Usage:
 * @code {c++}
 * struct AutoCounter {};
 *
 * COUNTER_INC(AutoCounter);
 * struct Foo
 * {
 *     static constexpr size_t ID = COUNTER_READ(AutoCounter);
 * };
 *
 * COUNTER_INC(AutoCounter);
 * struct Bar
 * {
 *     static constexpr size_t ID = COUNTER_READ(AutoCounter);
 * };
 * @endcode
 *
 *
 * @author ndx
 * @version 0.1
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <cstdint>
#include <type_traits>

namespace kb
{

#define COUNTER_READ_CRUMB(TAG, RANK, ACC) counter_crumb(TAG(), constant_index<RANK>(), constant_index<ACC>())
#define COUNTER_READ(TAG)                                                                                              \
    COUNTER_READ_CRUMB(                                                                                                \
        TAG, 1,                                                                                                        \
        COUNTER_READ_CRUMB(                                                                                            \
            TAG, 2,                                                                                                    \
            COUNTER_READ_CRUMB(                                                                                        \
                TAG, 4,                                                                                                \
                COUNTER_READ_CRUMB(                                                                                    \
                    TAG, 8,                                                                                            \
                    COUNTER_READ_CRUMB(                                                                                \
                        TAG, 16,                                                                                       \
                        COUNTER_READ_CRUMB(TAG, 32, COUNTER_READ_CRUMB(TAG, 64, COUNTER_READ_CRUMB(TAG, 128, 0))))))))

#define COUNTER_INC(TAG)                                                                                               \
    constexpr constant_index<COUNTER_READ(TAG) + 1> counter_crumb(                                                     \
        TAG, constant_index<(COUNTER_READ(TAG) + 1) & ~COUNTER_READ(TAG)>,                                             \
        constant_index<(COUNTER_READ(TAG) + 1) & COUNTER_READ(TAG)>)                                                   \
    {                                                                                                                  \
        return {};                                                                                                     \
    }

#define COUNTER_LINK_NAMESPACE(NS) using NS::counter_crumb;

template <std::size_t n>
struct constant_index : std::integral_constant<std::size_t, n>
{
};

template <typename id, std::size_t rank, std::size_t acc>
constexpr constant_index<acc> counter_crumb(id, constant_index<rank>, constant_index<acc>)
{
    return {};
} // found by ADL via constant_index

} // namespace kb