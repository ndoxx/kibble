#pragma once

#include "policy/policy.h"

namespace kb
{
namespace memory
{

class HeapArea;

/**
 * @brief Organizes allocation and deallocation operations on a block of memory.
 * This policy-oriented design makes it possible to use very different allocation algorithms. Moreover, multiple
 * sanitization policies can be configured during instantiation, which enables bug tracking capabilities. These
 * sanitization policies default to their null-types, so the retail build can remain overhead free.
 *
 * Code inspired by:
 *     https://blog.molecular-matters.com/2011/07/05/memory-system-part-1/
 *
 * @tparam AllocatorT allocation policy, handles the details of allocation and deallocation
 * @tparam ThreadPolicyT thread guard policyto make an arena thread safe if needed
 * @tparam BoundsCheckerT bounds checking sanitizer
 * @tparam MemoryTaggerT memory tagging sanitizer
 * @tparam MemoryTrackerT memory tracking sanitizer
 */
template <typename AllocatorT, typename ThreadPolicyT = policy::SingleThread,
          typename BoundsCheckerT = policy::NoBoundsChecking, typename MemoryTaggerT = policy::NoMemoryTagging,
          typename MemoryTrackerT = policy::NoMemoryTracking>
class MemoryArena
{
public:
    typedef uint32_t SIZE_TYPE;
    // Size of bookkeeping data before user pointer
    static constexpr uint32_t BK_FRONT_SIZE =
        policy::BoundsCheckerSentinelSize<BoundsCheckerT>::FRONT + sizeof(SIZE_TYPE);
    static constexpr uint32_t DECORATION_SIZE = BK_FRONT_SIZE + policy::BoundsCheckerSentinelSize<BoundsCheckerT>::BACK;

    /**
     * @brief Forwards all the arguments to the allocator's constructor.
     *
     * @tparam ArgsT types of the allocator's constructor arguments
     * @param args the allocator's constructor arguments
     */
    template <typename... ArgsT>
    MemoryArena(const char* debug_name, HeapArea& area, ArgsT&&... args)
        : allocator_(debug_name, area, DECORATION_SIZE, std::forward<ArgsT>(args)...)
    {
        if constexpr (policy::is_active_memory_tracking_policy<MemoryTrackerT>)
            memory_tracker_.init(debug_name, area);
    }

    ~MemoryArena()
    {
        if constexpr (policy::is_active_memory_tracking_policy<MemoryTrackerT>)
            memory_tracker_.report();
    }

    /**
     * @brief Get the allocator
     *
     * @return AllocatorT&
     */
    inline AllocatorT& get_allocator()
    {
        return allocator_;
    }

    /**
     * @brief Get the allocator as a const reference
     *
     * @return const AllocatorT&
     */
    inline const AllocatorT& get_allocator() const
    {
        return allocator_;
    }

    /**
     * @brief Allocate a memory chunk of a given size.
     *
     * This function handles alignment requirements. Some data may need to be written before the user pointer for
     * sanitization or array deallocation purposes, so the user pointer may be offset relative to the base
     * pointer returned by this function. The user pointer is the one to be aligned.
     *
     * @note This function may be a sync point, depending on the thread guard policy.
     *
     * @param size size of the chunk to allocate
     * @param alignment alignment constraint, such that `(returned_pointer + offset) % alignment == 0`
     * @param offset offset between the returned base pointer and the user pointer
     * @param file source file where that allocation was performed
     * @param line source line where that allocation was performed
     * @return base pointer to the allocated chunk
     */
    [[nodiscard]] void* allocate(size_t size, size_t alignment, size_t offset, [[maybe_unused]] const char* file,
                                 [[maybe_unused]] int line) noexcept
    {
        // Lock resource
        if constexpr (policy::is_active_threading_policy<ThreadPolicyT>)
            thread_guard_.enter();

        // Compute size after decoration and allocate
        const size_t decorated_size = DECORATION_SIZE + size;
        const size_t user_offset = BK_FRONT_SIZE + offset;

        uint8_t* begin = static_cast<uint8_t*>(allocator_.allocate(decorated_size, alignment, user_offset));

        uint8_t* current = begin;

        // Put front sentinel for overwrite detection
        if constexpr (policy::is_active_bounds_checking_policy<BoundsCheckerT>)
        {
            bounds_checker_.put_sentinel_front(current);
            current += policy::BoundsCheckerSentinelSize<BoundsCheckerT>::FRONT;
        }

        // Save allocation size
        *(reinterpret_cast<SIZE_TYPE*>(current)) = static_cast<SIZE_TYPE>(decorated_size);
        current += sizeof(SIZE_TYPE);

        // More bookkeeping
        if constexpr (policy::is_active_memory_tagging_policy<MemoryTaggerT>)
            memory_tagger_.tag_allocation(current, size);
        if constexpr (policy::is_active_bounds_checking_policy<BoundsCheckerT>)
            bounds_checker_.put_sentinel_back(current + size);
        if constexpr (policy::is_active_memory_tracking_policy<MemoryTrackerT>)
            memory_tracker_.on_allocation(begin, decorated_size, alignment, file, line);

        // Unlock resource and return user pointer
        if constexpr (policy::is_active_threading_policy<ThreadPolicyT>)
            thread_guard_.leave();

        return current;
    }

    /**
     * @brief Deallocate a chunk of memory.
     *
     * @note This function may be a sync point, depending on the thread guard policy.
     *
     * @param ptr
     */
    void deallocate(void* ptr, [[maybe_unused]] const char* file, [[maybe_unused]] int line) noexcept
    {
        if constexpr (policy::is_active_threading_policy<ThreadPolicyT>)
            thread_guard_.enter();

        // Take care to jump further back if non-POD array deallocation, because we also stored the number of instances
        uint8_t* begin = static_cast<uint8_t*>(ptr) - BK_FRONT_SIZE;

        // Check the front sentinel before we retrieve the allocation size, just in case
        // the size was corrupted by an overwrite.
        if constexpr (policy::is_active_bounds_checking_policy<BoundsCheckerT>)
            bounds_checker_.check_sentinel_front(begin);

        const SIZE_TYPE decorated_size =
            *(reinterpret_cast<SIZE_TYPE*>(begin + policy::BoundsCheckerSentinelSize<BoundsCheckerT>::FRONT));

        // Check that everything went ok
        if constexpr (policy::is_active_memory_tagging_policy<MemoryTaggerT>)
            memory_tagger_.tag_deallocation(begin, decorated_size);
        if constexpr (policy::is_active_bounds_checking_policy<BoundsCheckerT>)
        {
            bounds_checker_.check_sentinel_back(begin + decorated_size -
                                                policy::BoundsCheckerSentinelSize<BoundsCheckerT>::BACK);
        }
        if constexpr (policy::is_active_memory_tracking_policy<MemoryTrackerT>)
            memory_tracker_.on_deallocation(begin, decorated_size, file, line);

        allocator_.deallocate(begin);

        if constexpr (policy::is_active_threading_policy<ThreadPolicyT>)
            thread_guard_.leave();
    }

    /**
     * @brief Reset the allocator.
     *
     * @note This function may be a sync point, depending on the thread guard policy.
     *
     */
    inline void reset()
    {
        if constexpr (policy::is_active_threading_policy<ThreadPolicyT>)
            thread_guard_.enter();

        allocator_.reset();

        if constexpr (policy::is_active_threading_policy<ThreadPolicyT>)
            thread_guard_.leave();
    }

    /**
     * @brief Allocate an array of elements in an arena.
     *
     * @todo Use std::source_location when possible
     *
     * @tparam T type of object to allocate
     * @param N the array size
     * @param alignment memory alignment constraint for the user pointer
     * @param file the source file that performed this array allocation
     * @param line the source line that performed this array allocation
     * @return T*
     */
    template <typename T>
    T* new_array__(size_t N, size_t alignment, const char* file, int line)
    {
        if constexpr (std::is_standard_layout_v<T> && std::is_trivial_v<T>)
        {
            // Only need to align the base, size requirements will do the rest
            return static_cast<T*>(allocate(sizeof(T) * N, alignment, 0, file, line));
        }
        else
        {
            // new[] operator stores the number of instances in the first 4 bytes and
            // returns a pointer to the address right after, we emulate this behavior here.
            void* ptr = allocate(sizeof(T) * N + sizeof(SIZE_TYPE), alignment, sizeof(SIZE_TYPE), file, line);
            *static_cast<SIZE_TYPE*>(ptr) = SIZE_TYPE(N);

            // First object starts right after
            T* as_T = reinterpret_cast<T*>(static_cast<uint8_t*>(ptr) + sizeof(SIZE_TYPE));
            // Construct instances using placement new
            // NOTE(ndx): The parentheses of T() are important, without them the constructor is not called
            const T* const end = as_T + N;
            while (as_T < end)
                ::new (as_T++) T();

            // Hand user the pointer to the first instance
            return (as_T - N);
        }
    }

    // No "placement-delete" in C++, so we define this helper deleter
    /**
     * @brief Helper deleter.
     * There is no notion of "placement-delete" in C++, so this function is needed so as to call the object's destructor
     * in case it is not of trivial type.
     *
     * @bug Sometimes, the address returned by K_NEW is not the same as the one passed to the underlying placement new
     * (the "user" address computed by the allocator). I suspect pointer type casting is responsible (as placement new
     * is required to forward the input pointer), but I can't seem to pinpoint how it plays. I observe pointer mismatch
     * when the constructed type uses multiple inheritance. My fix is to dynamic cast the pointer to void* if the type
     * is polymorphic. Spoiler: it doesn't always work... When the base type holds a std::vector as a member for
     * example, it still fails, for reasons... See:
     * https://stackoverflow.com/questions/41246633/placement-new-crashing-when-used-with-virtual-inheritance-hierarchy-in-visual-c
     * UPDATE: many things have changed since then, thing bug may not even be a thing anymore.
     *
     * @todo Use std::source_location when possible
     *
     * @tparam T type of object to delete
     * @param object pointer to the object to delete
     */
    template <typename T>
    void delete__(T* object, const char* file, int line)
    {
        if constexpr (!(std::is_standard_layout_v<T> && std::is_trivial_v<T>))
            object->~T();

        if constexpr (std::is_polymorphic_v<T>)
            deallocate(dynamic_cast<void*>(object), file, line);
        else
            deallocate(object, file, line);
    }

    /**
     * @brief Helper deleter to delete arrays.
     *
     * @todo Use std::source_location when possible
     *
     * @see Delete()
     * @tparam T type of object to delete
     * @param object pointer to the object to delete
     */
    template <typename T>
    void delete_array__(T* object, const char* file, int line)
    {
        if constexpr (std::is_standard_layout_v<T> && std::is_trivial_v<T>)
        {
            deallocate(object, file, line);
        }
        else
        {
            // Number of instances stored sizeof(SIZE_TYPE) bytes before first instance
            SIZE_TYPE* as_usize = reinterpret_cast<SIZE_TYPE*>(object);
            const uint32_t N = as_usize[-1];

            // Call instances' destructor in reverse order
            for (SIZE_TYPE ii = N; ii > 0; --ii)
                object[ii - 1].~T();

            // deallocate() expects a pointer to the beginning of array allocation (where we wrote N)
            // NOTE(ndx): EXPECT deallocation bug when T is polymorphic, see HACK comment in Delete()
            // TODO: Test and fix this
            deallocate(as_usize - 1, file, line);
        }
    }

private:
    AllocatorT allocator_;
    ThreadPolicyT thread_guard_;
    BoundsCheckerT bounds_checker_;
    MemoryTaggerT memory_tagger_;
    MemoryTrackerT memory_tracker_;
};

namespace detail
{
/**
 * @brief Base helper template to extract type and element count informations from arrays.
 *
 * @tparam T element type
 */
template <class T>
struct TypeAndCount
{
};

/**
 * @brief Specialization of TypeAndCount with concrete implementation.
 *
 * @tparam T
 * @tparam N
 */
template <class T, size_t N>
struct TypeAndCount<T[N]>
{
    typedef T type;
    static constexpr size_t count = N;
};
} // namespace detail

} // namespace memory

/**
 * @def K_NEW(TYPE, ARENA)
 * Allocate an object of type TYPE in arena ARENA. Example:\n
 * `Obj* obj = K_NEW(Obj, arena);`
 */
#define K_NEW(TYPE, ARENA) ::new (ARENA.allocate(sizeof(TYPE), alignof(TYPE), 0, __FILE__, __LINE__)) TYPE

/**
 * @def K_NEW_ALIGN(TYPE, ARENA, ALIGNMENT)
 * Allocate an object of type TYPE in arena ARENA, the returned pointer will be aligned. Example:\n
 * `Obj* obj = K_NEW_ALIGN(Obj, arena, 4);`
 */
#define K_NEW_ALIGN(TYPE, ARENA, ALIGNMENT) ::new (ARENA.allocate(sizeof(TYPE), ALIGNMENT, 0, __FILE__, __LINE__)) TYPE

/**
 * @def K_NEW_ARRAY(TYPE, ARENA)
 * Allocate an array of objects in arena ARENA. The number of objects is know an compile-time. Example:\n
 * `Obj* obj_array = K_NEW_ARRAY(Obj[42], arena);`
 */
#define K_NEW_ARRAY(TYPE, ARENA)                                                                                       \
    ARENA.new_array__<kb::memory::detail::TypeAndCount<TYPE>::type>(                                                   \
        kb::memory::detail::TypeAndCount<TYPE>::count, alignof(kb::memory::detail::TypeAndCount<TYPE>::type),          \
        __FILE__, __LINE__)

/**
 * @def K_NEW_ARRAY_DYNAMIC(TYPE, COUNT, ARENA)
 * Allocate an array of objects in arena ARENA. The number of objects is known at run-time. Example:\n
 * `Obj* obj_array = K_NEW_ARRAY_DYNAMIC(Obj, num_objects, arena);`
 */
#define K_NEW_ARRAY_DYNAMIC(TYPE, COUNT, ARENA) ARENA.new_array__<TYPE>(COUNT, alignof(TYPE), __FILE__, __LINE__)

/**
 * @def K_NEW_ARRAY_ALIGN(TYPE, ARENA, ALIGNMENT)
 * Allocate an array of objects in arena ARENA. The number of objects is know an compile-time.
 * The returned pointer will be aligned. Example:\n
 * `Obj* obj_array = K_NEW_ARRAY_ALIGN(Obj[42], arena, 4);`
 */
#define K_NEW_ARRAY_ALIGN(TYPE, ARENA, ALIGNMENT)                                                                      \
    ARENA.new_array__<kb::memory::detail::TypeAndCount<TYPE>::type>(kb::memory::detail::TypeAndCount<TYPE>::count,     \
                                                                    ALIGNMENT, __FILE__, __LINE__)

/**
 * @def K_NEW_ARRAY_DYNAMIC_ALIGN(TYPE, COUNT, ARENA, ALIGNMENT)
 * Allocate an array of objects in arena ARENA. The number of objects is known at run-time.
 * The returned pointer will be aligned. Example:\n
 * `Obj* obj_array = K_NEW_ARRAY_DYNAMIC_ALIGN(Obj, num_objects, arena, 4);`
 */
#define K_NEW_ARRAY_DYNAMIC_ALIGN(TYPE, COUNT, ARENA, ALIGNMENT)                                                       \
    ARENA.new_array__<TYPE>(COUNT, ALIGNMENT, __FILE__, __LINE__)

/**
 * @def K_DELETE(OBJECT, ARENA)
 * Delete an object OBJECT from arena ARENA. Example:\n
 * `K_DELETE(obj_ptr, arena);`
 */
#define K_DELETE(OBJECT, ARENA) ARENA.delete__(OBJECT, __FILE__, __LINE__)

/**
 * @def K_DELETE_ARRAY(OBJECT, ARENA)
 * Delete an array of objects OBJECT from arena ARENA. Example:\n
 * `K_DELETE_ARRAY(obj_array_ptr, arena);`
 */
#define K_DELETE_ARRAY(OBJECT, ARENA) ARENA.delete_array__(OBJECT, __FILE__, __LINE__)

} // namespace kb