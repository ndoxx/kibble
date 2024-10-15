#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kb::log
{
class Channel;
}

namespace kb::memory
{

namespace debug
{

/**
 * @brief Describe a block inside a heap area.
 * For debug purposes, to visualize the current state of a HeapArea.
 *
 */
struct AreaItem
{
    /// debug name of the block
    std::string name;
    /// pointer to the beginning of the block
    void* begin = nullptr;
    /// pointer past the end of the block
    void* end = nullptr;
    /// size of the block
    std::size_t size = 0;
};

} // namespace debug

/**
 * @brief Memory resource used by memory arenas.
 * A HeapArea is just a big range of memory allocated on the heap. The memory arenas are given a block of a given size
 * belonging to a heap area, and are responsible for the memory management strategy and allocation operations on this
 * block.
 *
 */
class HeapArea
{
public:
    HeapArea() = default;

    /**
     * @brief Create a heap area by allocating a big contiguous chunk of memory on the heap.
     * If the symbol K_USE_MEM_AREA_MEMSET is defined, memset will be called on the whole area to initialize every
     * byte of it to a given value, specified by the k_area_memset_byte constant defined in config.h
     * If an allocation error happened, the constructor will throw a std::bad_alloc exception.
     *
     * @param size size of the area
     */
    explicit HeapArea(size_t size, const kb::log::Channel* channel = nullptr);

    /**
     * @brief Free the whole area.
     *
     */
    ~HeapArea();

    /**
     * @brief Get a pointer to the beginning of the area.
     *
     * @return void*
     */
    inline void* begin() const
    {
        return begin_;
    }

    /**
     * @brief Get a pointer past the last byte of the area.
     *
     * @return void*
     */
    inline void* end() const
    {
        return begin_ + size_ + 1;
    }

    /**
     * @brief Get both pointers returned by begin() and end() in one go.
     *
     * @return std::pair<void *, void *>
     */
    inline std::pair<void*, void*> range()
    {
        return {begin(), end()};
    }

    /**
     * @brief Reserve a memory block within this area, and advance the head.
     * The block is page-aligned so as to avoid false sharing if multiple threads share access to this area, this means
     * that a certain extent of the memory past the last head position will be padded. If the symbol
     * K_USE_MEM_MARK_PADDING is defined, the padded zone will be memset with a specific pattern defined as
     * k_alignment_padding_mark in config.h. This padding pattern could potentially be checked back afterwards to ensure
     * no allocation leaked past the block boundaries.
     *
     * @param size size of the block to reserve
     * @param debug_name name of the block used for debugging purposes
     * @return range of pointers marking the bounds of the reserved block
     */
    std::pair<void*, void*> require_block(size_t size, const char* debug_name = nullptr);

    /**
     * @brief Show the content of the area using the logger.
     * The channel "memory" must be setup for this to work.
     *
     */
    void debug_show_content();

    /**
     * @brief Show a hex dump of a portion of the memory content of this area, starting at the beginning and of
     * specified size.
     * @warning This was done in a hurry to debug something during the tests, the interface is bad and this should
     * evolve into a better designed feature later on.
     *
     * @param size size of the dump
     */
    void debug_hex_dump(size_t size = 0);

    /**
     * @brief Get the block allocation journal.
     *
     * @return a vector containing small debug::AreaItem structs describing each block
     */
    inline const std::vector<debug::AreaItem>& get_block_descriptions() const
    {
        return items_;
    }

    // clang-format off
    /// @brief Get total size in bytes
    inline size_t total_size() const{ return size_; }
    /// @brief Get remaining size in bytes
    inline size_t free_size() const{ return static_cast<uint64_t>(static_cast<uint8_t*>(end()) - head_); }
    // clang-format on

    /**
     * @brief Fill this whole area with a specified byte value.
     *
     * @param filler
     */
    inline void fill(uint8_t filler)
    {
        std::fill(begin_, begin_ + size_, filler);
    }

    inline const kb::log::Channel* get_logger_channel() const
    {
        return log_channel_;
    }

private:
    size_t size_;
    uint8_t* begin_;
    uint8_t* head_;

    std::vector<debug::AreaItem> items_; // for debug
    const kb::log::Channel* log_channel_ = nullptr;
};

} // namespace kb::memory