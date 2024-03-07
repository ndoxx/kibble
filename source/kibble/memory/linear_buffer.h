#pragma once

#include <cstring>
#include <string>

#include "policy/policy.h"
#include "../logger2/logger.h"

namespace kb::memory
{

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

} // namespace kb::memory