#pragma once

#include <array>
#include <cassert>
#include <numeric>
#include <type_traits>
#include <vector>

#include "../assert/assert.h"

namespace kb
{

/**
 * @brief Sparse set data structure.
 * This is an efficient way to represent a set of integers that can take any value within certain bounds but the values
 * remain sparse. Lookup and insertion are O(1) operations and iteration is O(n), with n the size of the set. Of course,
 * what it gives in temporal complexity, it takes in spatial complexity. Two fixed-size arrays are used internally that
 * may represent an important memory overhead. The maximum element count (which is the same as the maximum value) also
 * needs to be known beforehand.
 *
 * For a good use case of sparse sets, read the excellent articles of Skypjack (the author of EnTT):
 * - https://skypjack.github.io/2019-03-07-ecs-baf-part-2/
 * - https://skypjack.github.io/2019-09-25-ecs-baf-part-5/
 *
 * @tparam T integer type
 * @tparam SIZE maximum element count / value
 */
template <typename T, size_t SIZE>
class SparseSet
{
    static_assert(std::is_unsigned<T>::value, "SparseSet can only contain unsigned integers");

private:
    using Array = std::array<T, SIZE>;
    Array dense;
    Array sparse; // Map of elements to dense set indices

    size_t size_ = 0; // Element count

public:
    using iterator = typename Array::const_iterator;
    using const_iterator = typename Array::const_iterator;

    /**
     * @brief Return an iterator to the beginning of the sparse set.
     * This makes this structure iterable.
     *
     * @return iterator
     */
    inline iterator begin()
    {
        return dense.begin();
    }

    /**
     * @brief Return an iterator past the end of the sparse set.
     * This makes this structure iterable.
     *
     * @return iterator
     */
    inline iterator end()
    {
        return dense.begin() + size_;
    }

    /**
     * @brief Return a const iterator to the beginning of the sparse set.
     * This makes this structure iterable.
     *
     * @return const_iterator
     */
    inline const_iterator begin() const
    {
        return dense.begin();
    }

    /**
     * @brief Return a const iterator past the end of the sparse set.
     * This makes this structure iterable.
     *
     * @return const_iterator
     */
    inline const_iterator end() const
    {
        return dense.begin() + size_;
    }

    /**
     * @brief Return the number of integers in the set.
     *
     * @return size_t
     */
    inline size_t size() const
    {
        return size_;
    }

    /**
     * @brief Check if the set is empty.
     *
     * @return true if the set is empty
     * @return false otherwise
     */
    inline bool empty() const
    {
        return size_ == 0;
    }

    /**
     * @brief Remove all elements from the set.\n
     * Complexity: O(1)
     *
     */
    inline void clear()
    {
        size_ = 0;
    }

    /**
     * @brief Check if an element exists in the set.\n
     * Complexity: O(1)
     *
     * @param val value to check
     * @return true if val is in the set
     * @return false otherwise
     */
    inline bool has(const T &val) const
    {
        return val < SIZE && sparse[val] < size_ && dense[sparse[val]] == val;
    }

    /**
     * @brief Insert a new element in the set.
     * If the element already exists in the set, nothing is done.\n
     * Complexity: O(1)
     *
     * @param val value to insert
     */
    void insert(const T &val)
    {
        if (!has(val))
        {
            K_ASSERT(val < SIZE, "Exceeding SparseSet capacity", nullptr).watch(val).watch(SIZE);

            dense[size_] = val;
            sparse[val] = T(size_);
            ++size_;
        }
    }

    /**
     * @brief Remove an element in the set.
     * If the element does not exist in the set, nothing is done.\n
     * Complexity: O(1)
     *
     * @param val
     */
    void erase(const T &val)
    {
        if (has(val))
        {
            dense[sparse[val]] = dense[size_ - 1];
            sparse[dense[size_ - 1]] = sparse[val];
            --size_;
        }
    }
};

/**
 * @brief Growable SparseSet class.
 * One of the shortcomings of the SparseSet class is its compile-time fixed size. In some situations, the maximum number
 * of elements cannot be known in advance. This class addresses this issue by allowing its internal arrays to grow by
 * replacing them with vectors. This however leads to additional O(n) overhead when the vectors need to grow on
 * insertion.
 *
 * @tparam T integer type
 */
template <typename T>
class DynamicSparseSet
{
    static_assert(std::is_unsigned<T>::value, "SparseSet can only contain unsigned integers");

private:
    using Array = std::vector<T>;
    Array dense;
    Array sparse; // Map of elements to dense set indices

    size_t size_ = 0;     // Element count
    size_t capacity_ = 0; // Current capacity (maximum value + 1)

public:
    using iterator = typename Array::const_iterator;
    using const_iterator = typename Array::const_iterator;

    /**
     * @brief Return an iterator to the beginning of the sparse set.
     * This makes this structure iterable.
     *
     * @return iterator
     */
    inline iterator begin()
    {
        return dense.begin();
    }

    /**
     * @brief Return an iterator past the end of the sparse set.
     * This makes this structure iterable.
     *
     * @return iterator
     */
    inline iterator end()
    {
        return dense.begin() + size_;
    }

    /**
     * @brief Return a const iterator to the beginning of the sparse set.
     * This makes this structure iterable.
     *
     * @return const_iterator
     */
    inline const_iterator begin() const
    {
        return dense.begin();
    }

    /**
     * @brief Return a const iterator past the end of the sparse set.
     * This makes this structure iterable.
     *
     * @return const_iterator
     */
    inline const_iterator end() const
    {
        return dense.begin() + long(size_);
    }

    /**
     * @brief Return the number of integers in the set.
     *
     * @return size_t
     */
    inline size_t size() const
    {
        return size_;
    }

    /**
     * @brief Return the current capacity (maximum value +1).
     *
     * @return size_t
     */
    inline size_t capacity() const
    {
        return capacity_;
    }

    /**
     * @brief Check if the set is empty.
     *
     * @return true if the set is empty
     * @return false otherwise
     */
    inline bool empty() const
    {
        return size_ == 0;
    }

    /**
     * @brief Remove all elements from the set.
     * The capacity is conserved and now resizing happens.\n
     * Complexity: O(1)
     *
     */
    inline void clear()
    {
        size_ = 0;
    }

    /**
     * @brief Check if an element exists in the set.\n
     * Complexity: O(1)
     *
     * @param val value to check
     * @return true if val is in the set
     * @return false otherwise
     */
    inline bool has(const T &val) const
    {
        return val < capacity_ && sparse[val] < size_ && dense[sparse[val]] == val;
    }

    /**
     * @brief Reserve space for numbers up to u.
     * If the capacity is already greater than u, nothing happens.\n
     * Complexity: O(n) in the worst case if reallocation happens
     *
     * @param u new upper bound for the numbers
     */
    void reserve(size_t u)
    {
        if (u > capacity_)
        {
            dense.resize(u, 0);
            sparse.resize(u, 0);
            capacity_ = u;
        }
    }

    /**
     * @brief Insert a new element in the set.
     * If the element already exists in the set, nothing is done.\n
     * Complexity: O(1) in most cases, O(n) in the worst case if reallocation happens
     *
     * @param val value to insert
     */
    void insert(const T &val)
    {
        if (!has(val))
        {
            if (val >= capacity_)
                reserve(val + 1);

            dense[size_] = val;
            sparse[val] = T(size_);
            ++size_;
        }
    }

    /**
     * @brief Remove an element in the set.
     * If the element does not exist in the set, nothing is done. Internal vectors do not shrink.\n
     * Complexity: O(1)
     *
     * @param val
     */
    void erase(const T &val)
    {
        if (has(val))
        {
            dense[sparse[val]] = dense[size_ - 1];
            sparse[dense[size_ - 1]] = sparse[val];
            --size_;
        }
    }
};

/**
 * @brief Variation of the SparseSet where integers can be requested and released.
 * This structure is useful to create a handle system. Integers produced by this structure are unique. An integer that
 * has been produced by the pool will be called a "valid handle". Once an integer has been returned to the pool, it will
 * no longer be valid, and will be available again for reuse. The size must be known at compile-time.
 *
 * @tparam T integer type
 * @tparam SIZE maximum element count / value
 */
template <typename T, size_t SIZE>
class SparsePool
{
    static_assert(std::is_unsigned<T>::value, "SparsePool can only contain unsigned integers");

private:
    using Array = std::array<T, SIZE>;
    Array dense;
    Array sparse; // Map of elements to dense set indices

    size_t size_ = 0; // Element count

public:
    using iterator = typename Array::const_iterator;
    using const_iterator = typename Array::const_iterator;

    /**
     * @brief Construct a new Sparse Pool.
     * All numbers within bounds are available.
     *
     */
    SparsePool() : size_(0)
    {
        clear();
    }

    /**
     * @brief Return an iterator to the beginning of the sparse set.
     * This makes this structure iterable.
     *
     * @return iterator
     */
    inline iterator begin()
    {
        return dense.begin();
    }

    /**
     * @brief Return an iterator past the end of the sparse set.
     * This makes this structure iterable.
     *
     * @return iterator
     */
    inline iterator end()
    {
        return dense.begin() + size_;
    }

    /**
     * @brief Return a const iterator to the beginning of the sparse set.
     * This makes this structure iterable.
     *
     * @return const_iterator
     */
    inline const_iterator begin() const
    {
        return dense.begin();
    }

    /**
     * @brief Return a const iterator past the end of the sparse set.
     * This makes this structure iterable.
     *
     * @return const_iterator
     */
    inline const_iterator end() const
    {
        return dense.begin() + size_;
    }

    /**
     * @brief Return the number of integers in the set.
     *
     * @return size_t
     */
    inline size_t size() const
    {
        return size_;
    }

    /**
     * @brief Check if the set is empty.
     *
     * @return true if the set is empty
     * @return false otherwise
     */
    inline bool empty() const
    {
        return size_ == 0;
    }

    /**
     * @brief Check if an input integer was produced by this pool.\n
     * Complexity: O(1)
     *
     * @param val value to check
     * @return true if val was produced by this pool
     * @return false otherwise
     */
    inline bool is_valid(const T &val) const
    {
        return val < SIZE && sparse[val] < size_ && dense[sparse[val]] == val;
    }

    /**
     * @brief Remove all elements from the set.\n
     * Complexity: O(n)
     *
     */
    void clear()
    {
        size_ = 0;
        std::iota(dense.begin(), dense.end(), 0);
    }

    /**
     * @brief Produce the next available integer from the pool.
     * This integer will be deemed "valid" by this pool until it is returned to it via the release() function.\n
     * Complexity: O(1)
     *
     * @see release()
     */
    T acquire()
    {
        K_ASSERT(size_ + 1 < SIZE, "Exceeding SparsePool capacity", nullptr).watch(size_).watch(SIZE);

        T index = T(size_++);
        T handle = dense[index];
        sparse[handle] = index;

        return handle;
    }

    /**
     * @brief Return a valid handle to this pool.
     * The integer will no longer be valid until it is produced again via a call to acquire().\n
     * Complexity: O(1)
     *
     * @param handle
     */
    void release(T handle)
    {
        K_ASSERT(is_valid(handle), "Cannot release unknown handle", nullptr).watch(handle);

        T index = sparse[handle];
        T temp = dense[--size_];
        dense[size_] = handle;
        sparse[temp] = index;
        dense[index] = temp;
    }
};

namespace detail
{
/**
 * @internal
 * @brief Generate a binary mask at compile-time.
 *
 * @tparam UIntT type of unsigned integer
 * @param shift_pos bit position after which all bits are turned to 1 in the mask
 * @return constexpr UIntT bit mask
 */
template <typename UIntT, typename = std::enable_if_t<std::is_unsigned_v<UIntT>>>
constexpr UIntT make_mask(const UIntT shift_pos)
{
    UIntT result = 0;

    for (UIntT ii = shift_pos; ii < sizeof(UIntT) * UIntT(8); ++ii)
        result |= UIntT(1u) << ii;

    return result;
}
} // namespace detail

/**
 * @brief More robust version of the SparsePool.
 * One of the shortcomings of the SparsePool class is that a handle that has been released will be produced again as-is.
 * This makes it impractical to use in some real world scenarios, where we may want to distinguish a recycled handle
 * from a newly produced one: a resource indexed by an integer handle could have been freed, the handle released and
 * produced again later on, and we wouldn't notice by looking at the original handle.
 *
 * This version of the sparse pool uses a few MSB bits of the integer handles it produces to save the number of
 * recyclings. Then an old handle that has been released will never compare to a recycled handle that is using the same
 * base integer, at least until the counter overflows and cycles back to zero. The number of guard bits is adjustable at
 * compile-time thanks to a template parameter.
 *
 *
 * @tparam T integer type
 * @tparam SIZE maximum element count
 * @tparam GUARD_BITS number of bits in the guard
 */
template <typename T, size_t SIZE, size_t GUARD_BITS>
class SecureSparsePool
{
    static_assert(std::is_unsigned<T>::value, "SecureSparsePool can only contain unsigned integers");

private:
    using Array = std::array<T, SIZE>;
    Array dense;
    Array sparse; // Map of elements to dense set indices
    Array guard;  // Auto-incremented when index reused

    size_t size_ = 0; // Element count

public:
    using iterator = typename Array::const_iterator;
    using const_iterator = typename Array::const_iterator;
    static constexpr T k_guard_shift = sizeof(T) * T(8u) - GUARD_BITS;
    static constexpr T k_guard_mask = detail::make_mask<T>(k_guard_shift);
    static constexpr T k_handle_mask = ~k_guard_mask;

    /**
     * @brief Construct a new Sparse Pool.
     * All numbers within bounds are available.
     *
     */
    SecureSparsePool() : size_(0)
    {
        clear();
    }

    /**
     * @brief Return the base integer part of the handle.
     *
     * @param handle input guarded handle
     * @return T handle with guard bits all set to 0
     */
    static inline T unguard(T handle)
    {
        return k_handle_mask & handle;
    }

    /**
     * @brief Return the guard part of a handle.
     *
     * @param handle input guarded handle
     * @return T the guard value
     */
    static inline T guard_value(T handle)
    {
        return (k_guard_mask & handle) >> k_guard_shift;
    }

    /**
     * @brief Return an iterator to the beginning of the sparse set.
     * This makes this structure iterable.
     *
     * @return iterator
     */
    inline iterator begin()
    {
        return dense.begin();
    }

    /**
     * @brief Return an iterator past the end of the sparse set.
     * This makes this structure iterable.
     *
     * @return iterator
     */
    inline iterator end()
    {
        return dense.begin() + size_;
    }

    /**
     * @brief Return a const iterator to the beginning of the sparse set.
     * This makes this structure iterable.
     *
     * @return const_iterator
     */
    inline const_iterator begin() const
    {
        return dense.begin();
    }

    /**
     * @brief Return a const iterator past the end of the sparse set.
     * This makes this structure iterable.
     *
     * @return const_iterator
     */
    inline const_iterator end() const
    {
        return dense.begin() + size_;
    }

    /**
     * @brief Return the number of integers in the set.
     *
     * @return size_t
     */
    inline size_t size() const
    {
        return size_;
    }

    /**
     * @brief Check if the set is empty.
     *
     * @return true if the set is empty
     * @return false otherwise
     */
    inline bool empty() const
    {
        return size_ == 0;
    }

    /**
     * @brief Check if an input handle is valid.
     * The handle is deemed valid if:
     * - Its base integer part was produced by the pool
     * - The guard part is the most recent version for this base integer
     *
     * Complexity: O(1)
     *
     * @param val value to check
     * @return true if val is a valid handle
     * @return false otherwise
     */
    inline bool is_valid(const T &val) const
    {
        T unguarded = unguard(val);
        T guard_val = guard_value(val);
        return unguarded < SIZE && sparse[unguarded] < size_ && dense[sparse[unguarded]] == unguarded &&
               guard_val == guard[unguarded];
    }

    /**
     * @brief Remove all elements from the set.\n
     * Complexity: O(n)
     *
     */
    void clear()
    {
        size_ = 0;
        std::iota(dense.begin(), dense.end(), 0);
        std::fill(guard.begin(), guard.end(), 0);
    }

    /**
     * @brief Produce the next available handle from the pool.\n
     * Complexity: O(1)
     *
     * @see release()
     */
    T acquire()
    {
        K_ASSERT(size_ + 1 < SIZE, "Exceeding SecureSparsePool capacity", nullptr).watch(size_).watch(SIZE);

        T index = T(size_++);
        T unguarded = dense[index];
        sparse[unguarded] = index;

        return unguarded | (guard[unguarded] << k_guard_shift);
    }

    /**
     * @brief Return a valid handle to this pool.
     * The handle will no longer be valid as the guard part associated to the base integer is increased.\n
     * Complexity: O(1)
     *
     * @param handle
     */
    void release(T handle)
    {
        K_ASSERT(is_valid(handle), "Cannot release unknown handle", nullptr)
            .watch(size_t(unguard(handle)))
            .watch(size_t(guard_value(handle)));

        T unguarded = handle & k_handle_mask;
        T index = sparse[unguarded];
        T temp = dense[--size_] & k_handle_mask;
        dense[size_] = unguarded;
        sparse[temp] = index;
        dense[index] = temp;
        ++guard[unguarded];
    }
};

} // namespace kb