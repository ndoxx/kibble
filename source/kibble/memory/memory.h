#pragma once

#include <iomanip>
#include <iostream>

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include "../assert/assert.h"
#include "../logger2/logger.h"
#include "memory_utils.h"
#include "policy.h"

namespace kb
{
namespace memory
{

/**
 * @brief Organizes allocation and deallocation operations on a block of memory.
 * This policy-oriented design makes it possible to use very different allocation algorithms. Moreover, multiple
 * sanitization policies can be configured during instantiation, which enables bug tracking capabilities. These
 * sanitization policies default to their null-types, so the retail build can remain overhead free.
 *
 * Code inspired from::
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
    typedef uint32_t SIZE_TYPE; // BUG#4 if set to size_t/uint64_t, probably because of endianness
    // Size of bookkeeping data before user pointer
    static constexpr uint32_t BK_FRONT_SIZE = BoundsCheckerT::SIZE_FRONT + sizeof(SIZE_TYPE);
    static constexpr uint32_t DECORATION_SIZE = BK_FRONT_SIZE + BoundsCheckerT::SIZE_BACK;

    /**
     * @brief Construct a new Memory Arena for later lazy-initialization.
     *
     */
    MemoryArena() : is_initialized_(false)
    {
    }

    /**
     * @brief Forwards all the arguments to the allocator's constructor.
     *
     * @tparam ArgsT types of the allocator's constructor arguments
     * @param args the allocator's constructor arguments
     */
    template <typename... ArgsT>
    explicit MemoryArena(ArgsT&&... args) : allocator_(std::forward<ArgsT>(args)...), is_initialized_(true)
    {
    }

    ~MemoryArena()
    {
        shutdown();
    }

    /**
     * @brief Forwards all the arguments to the allocator's init function.
     *
     * @tparam ArgsT types of the allocator's init function arguments
     * @param args the allocator's init function arguments
     */
    template <typename... ArgsT>
    inline void init(ArgsT&&... args)
    {
        allocator_.init(std::forward<ArgsT>(args)...);
        is_initialized_ = true;
    }

    /**
     * @brief Set a logger channel instance for this arena
     *
     * @param log_channel
     */
    inline void set_logger_channel(const kb::log::Channel* log_channel)
    {
        log_channel_ = log_channel;
    }

    /**
     * @brief Call instead of dtor if arena is static or need to be reused
     *
     */
    inline void shutdown()
    {
        if (is_initialized_)
        {
            is_initialized_ = false;
            memory_tracker_.report(log_channel_);
        }
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
     * @brief Set the debug name
     *
     * @param name
     */
    inline void set_debug_name(const std::string& name)
    {
        debug_name_ = name;
    }

    /**
     * @brief Check if this arena is initialized
     *
     * @return true if the arena is initialized
     * @return false otherwise
     */
    inline bool is_initialized() const
    {
        return is_initialized_;
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
    void* allocate(size_t size, size_t alignment, size_t offset, [[maybe_unused]] const char* file,
                   [[maybe_unused]] int line)
    {
        // Lock resource
        thread_guard_.enter();

        // Compute size after decoration and allocate
        const size_t decorated_size = DECORATION_SIZE + size;
        const size_t user_offset = BK_FRONT_SIZE + offset;

        uint8_t* begin = static_cast<uint8_t*>(allocator_.allocate(decorated_size, alignment, user_offset));

        if (begin == nullptr)
        {
            klog(log_channel_).uid("Arena").fatal("\"{}\": Out of memory.", debug_name_);
        }

        uint8_t* current = begin;

        // Put front sentinel for overwrite detection
        bounds_checker_.put_sentinel_front(current);
        current += BoundsCheckerT::SIZE_FRONT;

        // Save allocation size
        *(reinterpret_cast<SIZE_TYPE*>(current)) = static_cast<SIZE_TYPE>(decorated_size);
        current += sizeof(SIZE_TYPE);

        // More bookkeeping
        memory_tagger_.tag_allocation(current, size);
        bounds_checker_.put_sentinel_back(current + size);
        memory_tracker_.on_allocation(begin, decorated_size, alignment, file, line);

        klog(log_channel_)
            .uid("Arena")
            .verbose(R"({} -- Allocation:
Decorated size: {}
Begin ptr:      {:#x}
User ptr:       {:#x})",
                     debug_name_, utils::human_size(decorated_size), reinterpret_cast<uint64_t>(begin),
                     reinterpret_cast<uint64_t>(current));

        // Unlock resource and return user pointer
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
    void deallocate(void* ptr)
    {
        thread_guard_.enter();
        // Take care to jump further back if non-POD array deallocation, because we also stored the number of instances
        uint8_t* begin = static_cast<uint8_t*>(ptr) - BK_FRONT_SIZE;

        // Check the front sentinel before we retrieve the allocation size, just in case
        // the size was corrupted by an overwrite.
        bounds_checker_.check_sentinel_front(begin);
        const SIZE_TYPE decorated_size = *(reinterpret_cast<SIZE_TYPE*>(begin + BoundsCheckerT::SIZE_FRONT));

        // Check that everything went ok
        bounds_checker_.check_sentinel_back(begin + decorated_size - BoundsCheckerT::SIZE_BACK);
        memory_tracker_.on_deallocation(begin);
        memory_tagger_.tag_deallocation(begin, decorated_size);

        allocator_.deallocate(begin);

        klog(log_channel_)
            .uid("Arena")
            .verbose(R"({} -- Deallocation:
Decorated size: {}
Begin ptr:      {:#x}
User ptr:       {:#x})",
                     debug_name_, utils::human_size(decorated_size), reinterpret_cast<uint64_t>(begin),
                     reinterpret_cast<uint64_t>(ptr));

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
        thread_guard_.enter();
        allocator_.reset();
        thread_guard_.leave();
    }

private:
    AllocatorT allocator_;
    ThreadPolicyT thread_guard_;
    BoundsCheckerT bounds_checker_;
    MemoryTaggerT memory_tagger_;
    MemoryTrackerT memory_tracker_;

    std::string debug_name_;
    bool is_initialized_;
    const kb::log::Channel* log_channel_ = nullptr;
};

/**
 * @brief Buffer on a heap area with a linear allocation scheme.
 * Similar to a MemoryArena with a LinearAllocator but with specialized read() and write() functions instead of
 * allocation functions. This buffer is random access, the head can be moved anywhere thanks to a seek() function.
 *
 * @todo This class could be rewritten as a wrapper around a MemoryArena with a custom linear allocator that enables
 * random access.
 *
 * @tparam ThreadPolicyT
 */
template <typename ThreadPolicyT = policy::SingleThread>
class LinearBuffer
{
public:
    LinearBuffer(const kb::log::Channel* log_channel = nullptr) : log_channel_(log_channel)
    {
    }

    /**
     * @brief Construct a new Linear Buffer of specified size.
     *
     * @param area target area to reserve a block from
     * @param size buffer size
     * @param debug_name name to use in debug mode
     */
    LinearBuffer(kb::memory::HeapArea& area, std::size_t size, const char* debug_name)
    {
        init(area, size, debug_name);
    }

    /**
     * @brief Lazy-initialize a Linear Buffer.
     *
     * @param area target area to reserve a block from
     * @param size buffer size
     * @param debug_name name to use in debug mode
     */
    inline void init(kb::memory::HeapArea& area, std::size_t size, const char* debug_name)
    {
        std::pair<void*, void*> range = area.require_block(size, debug_name);
        begin_ = static_cast<uint8_t*>(range.first);
        end_ = static_cast<uint8_t*>(range.second);
        head_ = begin_;
        debug_name_ = debug_name;
    }

    /**
     * @brief Set the debug name
     *
     * @param name
     */
    inline void set_debug_name(const std::string& name)
    {
        debug_name_ = name;
    }

    /**
     * @brief Copy data to this buffer.
     * The data will be written starting at the current head position, then the head will be incremented by the size of
     * the data written.
     *
     * @param source pointer to the beginning of the data
     * @param size size to copy
     */
    inline void write(void const* source, std::size_t size)
    {
        if (head_ + size >= end_)
        {
            klog(log_channel_).uid("LinearBuffer").fatal("\"{}\": Data buffer overwrite!", debug_name_);
        }
        memcpy(head_, source, size);
        head_ += size;
    }

    /**
     * @brief Copy data from this buffer.
     * The data will be read starting from the current position, then the head will be incremented by the size of
     * the data read.
     *
     * @param destination pointer to where the data should be copied to
     * @param size size to copy
     */
    inline void read(void* destination, std::size_t size)
    {
        if (head_ + size >= end_)
        {
            klog(log_channel_).uid("LinearBuffer").fatal("\"{}\": Data buffer overread!", debug_name_);
        }
        memcpy(destination, head_, size);
        head_ += size;
    }

    /**
     * @brief Convenience function to write an object of any type.
     *
     * @see write()
     * @tparam T type of the object to write
     * @param source pointer to the object to write
     */
    template <typename T>
    inline void write(T* source)
    {
        write(source, sizeof(T));
    }

    /**
     * @brief Convenience function to read an object of any type.
     *
     * @see read()
     * @tparam T type of the object to read
     * @param destination pointer to the memory location where the object should be copied
     */
    template <typename T>
    inline void read(T* destination)
    {
        read(destination, sizeof(T));
    }

    /**
     * @brief Specialized writing function for string objects.
     *
     * @param str the string to write
     */
    inline void write_str(const std::string& str)
    {
        uint32_t str_size = uint32_t(str.size());
        write(&str_size, sizeof(uint32_t));
        write(str.data(), str_size);
    }

    /**
     * @brief Specialized reading function for string objects.
     *
     * @param str the string to set
     */
    inline void read_str(std::string& str)
    {
        uint32_t str_size;
        read(&str_size, sizeof(uint32_t));
        str.resize(str_size);
        read(str.data(), str_size);
    }

    /**
     * @brief Reset the head position to the beginning of the block.
     *
     */
    inline void reset()
    {
        head_ = begin_;
    }

    /**
     * @brief Get the head position.
     *
     * @return uint8_t*
     */
    inline uint8_t* head()
    {
        return head_;
    }

    /**
     * @brief Get a pointer to the beginning of the block.
     *
     * @return uint8_t*
     */
    inline uint8_t* begin()
    {
        return begin_;
    }

    /**
     * @brief Get a const pointer to the beginning of the block.
     *
     * @return uint8_t*
     */
    inline const uint8_t* begin() const
    {
        return begin_;
    }

    /**
     * @brief Set head to the specified position.
     *
     * @param ptr target position
     */
    inline void seek(void* ptr)
    {
        K_ASSERT(ptr >= begin_, "Cannot seak before beginning of the block", log_channel_).watch(ptr).watch(begin_);
        K_ASSERT(ptr < end_, "Cannot seak after end of the block", log_channel_).watch(ptr).watch(end_);
        head_ = static_cast<uint8_t*>(ptr);
    }

private:
    uint8_t* begin_;
    uint8_t* end_;
    uint8_t* head_;
    std::string debug_name_;
    const kb::log::Channel* log_channel_ = nullptr;
};

/**
 * @brief Allocate an array of elements in an arena.
 *
 * @tparam T type of object to allocate
 * @tparam ArenaT type of the target arena
 * @param arena the arena this array should be allocated to
 * @param N the array size
 * @param alignment memory alignment constraint for the user pointer
 * @param file the source file that performed this array allocation
 * @param line the source line that performed this array allocation
 * @return T*
 */
template <typename T, class ArenaT>
T* NewArray(ArenaT& arena, size_t N, size_t alignment, const char* file, int line)
{
    if constexpr (std::is_standard_layout_v<T> && std::is_trivial_v<T>)
    {
        return static_cast<T*>(arena.allocate(sizeof(T) * N, alignment, 0, file, line));
    }
    else
    {
        // new[] operator stores the number of instances in the first 4 bytes and
        // returns a pointer to the address right after, we emulate this behavior here.
        uint32_t* as_uint = static_cast<uint32_t*>(
            arena.allocate(sizeof(uint32_t) + sizeof(T) * N, alignment, sizeof(uint32_t), file, line));
        *(as_uint++) = static_cast<uint32_t>(N);

        // Construct instances using placement new
        T* as_T = reinterpret_cast<T*>(as_uint);
        const T* const end = as_T + N;
        while (as_T < end)
            ::new (as_T++) T;

        // Hand user the pointer to the first instance
        return (as_T - N);
    }
}

// No "placement-delete" in C++, so we define this helper deleter
/**
 * @brief Helper deleter.
 * There is no notion of "placement-delete" in C++, so this function is needed so as to call the object's destructor in
 * case it is not of trivial type.
 *
 * @bug Sometimes, the address returned by K_NEW is not the same as the one passed to the underlying placement new (the
 * "user" address computed by the allocator). I suspect pointer type casting is responsible (as placement new is
 * required to forward the input pointer), but I can't seem to pinpoint how it plays. I observe pointer mismatch when
 * the constructed type uses multiple inheritance. My fix is to dynamic cast the pointer to void* if the type is
 * polymorphic. Spoiler: it doesn't always work... When the base type holds a std::vector as a member for example, it
 * still fails, for reasons... See:
 * https://stackoverflow.com/questions/41246633/placement-new-crashing-when-used-with-virtual-inheritance-hierarchy-in-visual-c
 *
 * @tparam T type of object to delete
 * @tparam ArenaT type of the target arena
 * @param object pointer to the object to delete
 * @param arena target arena reference
 */
template <typename T, class ArenaT>
void Delete(T* object, ArenaT& arena)
{
    if constexpr (!(std::is_standard_layout_v<T> && std::is_trivial_v<T>))
        object->~T();

    if constexpr (std::is_polymorphic_v<T>)
        arena.deallocate(dynamic_cast<void*>(object));
    else
        arena.deallocate(object);
}

/**
 * @brief Helper deleter to delete arrays.
 *
 * @see Delete()
 * @tparam T type of object to delete
 * @tparam ArenaT type of the target arena
 * @param object pointer to the object to delete
 * @param arena target arena reference
 */
template <typename T, class ArenaT>
void DeleteArray(T* object, ArenaT& arena)
{
    if constexpr (std::is_standard_layout_v<T> && std::is_trivial_v<T>)
    {
        arena.deallocate(object);
    }
    else
    {
        union {
            uint32_t* as_uint;
            T* as_T;
        };
        // User pointer points to first instance
        as_T = object;
        // Number of instances stored 4 bytes before first instance
        const uint32_t N = as_uint[-1];

        // Call instances' destructor in reverse order
        for (uint32_t ii = N; ii > 0; --ii)
            as_T[ii - 1].~T();

        // Arena's deallocate() expects a pointer 4 bytes before actual user pointer
        // NOTE(ndx): EXPECT deallocation bug when T is polymorphic, see HACK comment in Delete()
        // TODO: Test and fix this
        arena.deallocate(as_uint - 1);
    }
}

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

} // namespace memory

/**
 * @def K_NEW(TYPE, ARENA)
 * Allocate an object of type TYPE in arena ARENA. Example:\n
 * `Obj* obj = K_NEW(Obj, arena);`
 *
 * @def K_NEW_ARRAY(TYPE, ARENA)
 * Allocate an array of objects in arena ARENA. The number of objects is know an compile-time. Example:\n
 * `Obj* obj_array = K_NEW_ARRAY(Obj[42], arena);`
 *
 * @def K_NEW_ARRAY_DYNAMIC(TYPE, COUNT, ARENA)
 * Allocate an array of objects in arena ARENA. The number of objects is known at run-time. Example:\n
 * `Obj* obj_array = K_NEW_ARRAY_DYNAMIC(Obj, num_objects, arena);`
 *
 * @def K_NEW_ALIGN(TYPE, ARENA, ALIGNMENT)
 * Allocate an object of type TYPE in arena ARENA, the returned pointer will be aligned. Example:\n
 * `Obj* obj = K_NEW_ALIGN(Obj, arena, 4);`
 *
 * @def K_NEW_ARRAY_ALIGN(TYPE, ARENA, ALIGNMENT)
 * Allocate an array of objects in arena ARENA. The number of objects is know an compile-time.
 * The returned pointer will be aligned. Example:\n
 * `Obj* obj_array = K_NEW_ARRAY_ALIGN(Obj[42], arena, 4);`
 *
 * @def K_NEW_ARRAY_DYNAMIC_ALIGN(TYPE, COUNT, ARENA, ALIGNMENT)
 * Allocate an array of objects in arena ARENA. The number of objects is known at run-time.
 * The returned pointer will be aligned. Example:\n
 * `Obj* obj_array = K_NEW_ARRAY_DYNAMIC_ALIGN(Obj, num_objects, arena, 4);`
 *
 * @def K_DELETE(OBJECT, ARENA)
 * Delete an object OBJECT from arena ARENA. Example:\n
 * `K_DELETE(obj_ptr, arena);`
 *
 * @def K_DELETE_ARRAY(OBJECT, ARENA)
 * Delete an array of objects OBJECT from arena ARENA. Example:\n
 * `K_DELETE_ARRAY(obj_array_ptr, arena);`
 *
 */

#define K_NEW(TYPE, ARENA) ::new (ARENA.allocate(sizeof(TYPE), 0, 0, __FILE__, __LINE__)) TYPE
#define K_NEW_ARRAY(TYPE, ARENA)                                                                                       \
    kb::memory::NewArray<kb::memory::TypeAndCount<TYPE>::type>(ARENA, kb::memory::TypeAndCount<TYPE>::count, 0,        \
                                                               __FILE__, __LINE__)
#define K_NEW_ARRAY_DYNAMIC(TYPE, COUNT, ARENA) kb::memory::NewArray<TYPE>(ARENA, COUNT, 0, __FILE__, __LINE__)
#define K_NEW_ALIGN(TYPE, ARENA, ALIGNMENT) ::new (ARENA.allocate(sizeof(TYPE), ALIGNMENT, 0, __FILE__, __LINE__)) TYPE
#define K_NEW_ARRAY_ALIGN(TYPE, ARENA, ALIGNMENT)                                                                      \
    kb::memory::NewArray<kb::memory::TypeAndCount<TYPE>::type>(ARENA, kb::memory::TypeAndCount<TYPE>::count,           \
                                                               ALIGNMENT, __FILE__, __LINE__)
#define K_NEW_ARRAY_DYNAMIC_ALIGN(TYPE, COUNT, ARENA, ALIGNMENT)                                                       \
    kb::memory::NewArray<TYPE>(ARENA, COUNT, ALIGNMENT, __FILE__, __LINE__)
#define K_DELETE(OBJECT, ARENA) kb::memory::Delete(OBJECT, ARENA)
#define K_DELETE_ARRAY(OBJECT, ARENA) kb::memory::DeleteArray(OBJECT, ARENA)

// When this feature is implemented in C++, use source_location and something like:
/*
    #include <source_location>

    template <typename T, typename ArenaT, typename... Args>
    T* k_new(ArenaT& arena, Args... args,
             const std::source_location& location = std::source_location::current())
    {
        return new(arena.allocate(sizeof(T), 0, 0, location.file_name(), location.line()))(std::forward<Args>(args)...);
    }
*/

} // namespace kb