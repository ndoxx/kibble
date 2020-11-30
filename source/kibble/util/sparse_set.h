#pragma once

#include <array>
#include <cassert>
#include <numeric>
#include <type_traits>
#include <vector>

namespace kb
{

template <typename T, size_t SIZE> class SparseSet
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

    inline iterator begin() { return dense.begin(); }
    inline iterator end() { return dense.begin() + size_; }
    inline const_iterator begin() const { return dense.begin(); }
    inline const_iterator end() const { return dense.begin() + size_; }

    inline size_t size() const { return size_; }
    inline bool empty() const { return size_ == 0; }
    inline void clear() { size_ = 0; }
    inline bool has(const T& val) const { return val < SIZE && sparse[val] < size_ && dense[sparse[val]] == val; }

    void insert(const T& val)
    {
        if(!has(val))
        {
            K_ASSERT(val < SIZE, "Exceeding SparseSet capacity");

            dense[size_] = val;
            sparse[val] = T(size_);
            ++size_;
        }
    }

    void erase(const T& val)
    {
        if(has(val))
        {
            dense[sparse[val]] = dense[size_ - 1];
            sparse[dense[size_ - 1]] = sparse[val];
            --size_;
        }
    }
};

template <typename T> class DynamicSparseSet
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

    inline iterator begin() { return dense.begin(); }
    inline iterator end() { return dense.begin() + size_; }
    inline const_iterator begin() const { return dense.begin(); }
    inline const_iterator end() const { return dense.begin() + long(size_); }

    inline size_t size() const { return size_; }
    inline size_t capacity() const { return capacity_; }
    inline bool empty() const { return size_ == 0; }
    inline void clear() { size_ = 0; }
    inline bool has(const T& val) const { return val < capacity_ && sparse[val] < size_ && dense[sparse[val]] == val; }

    void reserve(size_t u)
    {
        if(u > capacity_)
        {
            dense.resize(u, 0);
            sparse.resize(u, 0);
            capacity_ = u;
        }
    }

    void insert(const T& val)
    {
        if(!has(val))
        {
            if(val >= capacity_)
                reserve(val + 1);

            dense[size_] = val;
            sparse[val] = T(size_);
            ++size_;
        }
    }

    void erase(const T& val)
    {
        if(has(val))
        {
            dense[sparse[val]] = dense[size_ - 1];
            sparse[dense[size_ - 1]] = sparse[val];
            --size_;
        }
    }
};

template <typename T, size_t SIZE> class SparsePool
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

    SparsePool() : size_(0) { clear(); }

    inline iterator begin() { return dense.begin(); }
    inline iterator end() { return dense.begin() + size_; }
    inline const_iterator begin() const { return dense.begin(); }
    inline const_iterator end() const { return dense.begin() + size_; }

    inline size_t size() const { return size_; }
    inline bool empty() const { return size_ == 0; }
    inline bool is_valid(const T& val) const { return val < SIZE && sparse[val] < size_ && dense[sparse[val]] == val; }

    void clear()
    {
        size_ = 0;
        std::iota(dense.begin(), dense.end(), 0);
    }

    T acquire()
    {
        K_ASSERT(size_ + 1 < SIZE, "Exceeding SparsePool capacity");

        T index = T(size_++);
        T handle = dense[index];
        sparse[handle] = index;

        return handle;
    }

    void release(T handle)
    {
        K_ASSERT(is_valid(handle), "Cannot release unknown handle");

        T index = sparse[handle];
        T temp = dense[--size_];
        dense[size_] = handle;
        sparse[temp] = index;
        dense[index] = temp;
    }
};

template <typename UIntT, typename = std::enable_if_t<std::is_unsigned_v<UIntT>>>
constexpr UIntT make_mask(const UIntT shift_pos)
{
    UIntT result = 0;

    for(UIntT ii = shift_pos; ii < sizeof(UIntT) * UIntT(8); ++ii)
        result |= UIntT(1u) << ii;

    return result;
}

template <typename T, size_t SIZE, size_t GUARD_BITS> class SecureSparsePool
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
    static constexpr T k_guard_mask = make_mask<T>(k_guard_shift);
    static constexpr T k_handle_mask = ~k_guard_mask;

    SecureSparsePool() : size_(0) { clear(); }

    static inline T unguard(T handle) { return k_handle_mask & handle; }
    static inline T guard_value(T handle) { return (k_guard_mask & handle) >> k_guard_shift; }

    inline iterator begin() { return dense.begin(); }
    inline iterator end() { return dense.begin() + size_; }
    inline const_iterator begin() const { return dense.begin(); }
    inline const_iterator end() const { return dense.begin() + size_; }

    inline size_t size() const { return size_; }
    inline bool empty() const { return size_ == 0; }
    inline bool is_valid(const T& val) const
    {
        T unguarded = unguard(val);
        T guard_val = guard_value(val);
        return unguarded < SIZE && sparse[unguarded] < size_ && dense[sparse[unguarded]] == unguarded && guard_val == guard[unguarded];
    }

    void clear()
    {
        size_ = 0;
        std::iota(dense.begin(), dense.end(), 0);
        std::fill(guard.begin(), guard.end(), 0);
    }

    T acquire()
    {
        K_ASSERT(size_ + 1 < SIZE, "Exceeding SecureSparsePool capacity");

        T index = T(size_++);
        T unguarded = dense[index];
        sparse[unguarded] = index;

        return unguarded | (guard[unguarded] << k_guard_shift);
    }

    void release(T handle)
    {
        K_ASSERT_FMT(is_valid(handle), "Cannot release unknown handle %lu/%lu", size_t(unguard(handle)), size_t(guard_value(handle)));

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