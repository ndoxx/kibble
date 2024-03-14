#include "memory/allocator/tlsf_allocator.h"
#include "assert/assert.h"
#include "memory/allocator/impl/tlsf_bit.h"

#include <cstddef>

using std::size_t;

namespace kb::memory
{

template <typename T1, typename T2>
inline size_t constexpr offset_of(T1 T2::*member)
{
    constexpr T2 object{};
    return size_t(&(object.*member)) - size_t(&object);
}

// * Constants
// log2 of linear subdivisions of block sizes. 4 - 5 are typical values. Larger values require more memory in the
// control structure.
constexpr size_t k_sl_index_count_log2 = 5ull;
// All allocation sizes and addresses are aligned to 8 bytes
constexpr size_t k_align_size_log2 = 3ull;
constexpr size_t k_align_size = 1ull << k_align_size_log2;
// Number of elements in the bitmaps
constexpr size_t k_fl_index_max = 32ull;
constexpr size_t k_sl_index_count = 1ull << k_sl_index_count_log2;
constexpr size_t k_fl_index_shift = k_sl_index_count_log2 + k_align_size_log2;
constexpr size_t k_fl_index_count = k_fl_index_max - k_fl_index_shift + 1ull;
constexpr size_t k_small_block_size = 1ull << k_fl_index_shift;

struct BlockHeader
{
    // * Data
    // Points to the previous physical block
    BlockHeader* prev_physical;
    // Size of this block, excluding header
    size_t size;
    // Next / previous free blocks (unused / overwritten if block is allocated)
    BlockHeader* next_free;
    BlockHeader* prev_free;

    // * Constants
    /*
        Block sizes are always at least a multiple of 4 (minimum allocation size is 4B),
        so we're free to use the two last bits of the size field to store block status:
        - bit 0: whether the block is free / busy (used)
        - bit 1: whether the previous block is free / busy
    */
    static constexpr size_t k_block_header_free_bit = 1ull << 0;
    static constexpr size_t k_block_header_prev_free_bit = 1ull << 1;
    /*
        The prev_physical field is stored *inside* the previous free block,
        and the last two intrusive pointers are overwritten by block data when block is busy.
        So the size of the block header as exposed to used blocks is just the size of the size field.
    */
    static constexpr size_t k_block_header_overhead = sizeof(size_t);

    /*
        User data starts directly after the size field in a busy block.
        Cannot use offsetof here (not constexpr), ASSUME the two fields are packed.
        It should be the case, a pointer to a struct is 8B, and the size member is
        a size_t which is 8B large and 8B aligned.
    */
    static constexpr size_t k_block_start_offset = sizeof(prev_physical) + sizeof(size);

    // * Functionality

    /**
     * @internal
     * @brief Get block size
     *
     * @return size_t
     */
    inline size_t block_size() const
    {
        // Remove the last two flag bits
        return size & ~(k_block_header_free_bit | k_block_header_prev_free_bit);
    }

    /**
     * @internal
     * @brief Set size of this block, without touching free bits
     *
     * @param size_
     */
    inline void set_size(size_t size_)
    {
        size = size_ | (size & (k_block_header_free_bit | k_block_header_prev_free_bit));
    }

    /**
     * @internal
     * @brief Check if this block has no next neighbor
     *
     * @return true
     * @return false
     */
    inline bool is_last() const
    {
        return block_size() == 0;
    }

    /**
     * @internal
     * @brief Check if this block is free
     *
     * @return true
     * @return false
     */
    inline bool is_free() const
    {
        return bool(size & k_block_header_free_bit);
    }

    /**
     * @internal
     * @brief Set free bit for this block
     *
     */
    inline void set_free()
    {
        size |= k_block_header_free_bit;
    }

    /**
     * @internal
     * @brief Clear free bit for this block
     *
     */
    inline void set_busy()
    {
        size &= ~k_block_header_free_bit;
    }

    /**
     * @internal
     * @brief Check if previous block is free
     *
     * @return true
     * @return false
     */
    inline bool is_prev_free() const
    {
        return bool(size & k_block_header_prev_free_bit);
    }

    /**
     * @internal
     * @brief Set free bit for previous block
     *
     */
    inline void set_prev_free()
    {
        size |= k_block_header_prev_free_bit;
    }

    /**
     * @internal
     * @brief Clear free bit for previous block
     *
     */
    inline void set_prev_busy()
    {
        size &= ~k_block_header_prev_free_bit;
    }

    /**
     * @internal
     * @brief Get a const block pointer from a const void pointer
     *
     * @param ptr
     * @return const BlockHeader*
     */
    static inline const BlockHeader* from_void_ptr(const void* ptr)
    {
        return reinterpret_cast<const BlockHeader*>(reinterpret_cast<const unsigned char*>(ptr) - k_block_start_offset);
    }

    /**
     * @internal
     * @brief Get a block pointer from a void pointer
     *
     * @param ptr
     * @return BlockHeader*
     */
    static inline BlockHeader* from_void_ptr(void* ptr)
    {
        return reinterpret_cast<BlockHeader*>(reinterpret_cast<unsigned char*>(ptr) - k_block_start_offset);
    }

    /**
     * @internal
     * @brief Convert this block to a const void pointer
     *
     * @return const void*
     */
    inline const void* to_void_ptr() const
    {
        return reinterpret_cast<const void*>(reinterpret_cast<const unsigned char*>(this) + k_block_start_offset);
    }

    /**
     * @internal
     * @brief Convert this block to a void pointer
     *
     * @return void*
     */
    inline void* to_void_ptr()
    {
        return reinterpret_cast<void*>(reinterpret_cast<unsigned char*>(this) + k_block_start_offset);
    }

    /**
     * @internal
     * @brief Return location of next block after block of given size
     *
     * @param ptr
     * @param size
     * @return BlockHeader*
     */
    static inline BlockHeader* offset_to_block(const void* ptr, size_t size)
    {
        return reinterpret_cast<BlockHeader*>(std::ptrdiff_t(ptr) + std::ptrdiff_t(size));
    }

    /**
     * @internal
     * @brief Return location of previous block
     *
     * @return BlockHeader*
     */
    inline BlockHeader* get_prev() const
    {
        K_ASSERT(is_prev_free(), "Previous block must be free", nullptr);
        return prev_physical;
    }

    /**
     * @internal
     * @brief Return location of next existing block
     *
     * @return BlockHeader*
     */
    inline BlockHeader* get_next() const
    {
        K_ASSERT(!is_last(), "Next block must be busy", nullptr);
        return offset_to_block(to_void_ptr(), block_size() - k_block_header_overhead);
    }

    /**
     * @internal
     * @brief Link a new block with its physical neighbor
     *
     * @return BlockHeader* the neighbor
     */
    inline BlockHeader* link_next()
    {
        BlockHeader* next = get_next();
        next->prev_physical = this;
        return next;
    }

    /**
     * @internal
     * @brief Link to next block and set this block free
     *
     */
    void mark_as_free()
    {
        // Link the block to the next block, first
        link_next()->set_prev_free();
        set_free();
    }

    /**
     * @internal
     * @brief Set this block as busy, inform next block
     *
     */
    void mark_as_busy()
    {
        get_next()->set_prev_busy();
        set_busy();
    }
};

/*
    A free block must be large enough to store its header minus the size of the prev_physical
    field, and no larger than the number of addressable bits for first level index.
*/
constexpr size_t k_block_size_min = sizeof(BlockHeader) - sizeof(BlockHeader*);
constexpr size_t k_block_size_max = 1ull << k_fl_index_max;

// * Alignment utils
inline bool is_pow2(size_t x)
{
    return (x & (x - 1)) == 0;
}

inline size_t align_up(size_t x, size_t alignment)
{
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2", nullptr);
    return (x + (alignment - 1)) & ~(alignment - 1);
}

inline size_t align_down(size_t x, size_t alignment)
{
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2", nullptr);
    return x - (x & (alignment - 1));
}

inline void* align_ptr(const void* ptr, size_t alignment)
{
    K_ASSERT(is_pow2(alignment), "alignment must be a power of 2", nullptr);
    const std::ptrdiff_t aligned =
        (std::ptrdiff_t(ptr) + (std::ptrdiff_t(alignment) - 1)) & ~(std::ptrdiff_t(alignment) - 1);
    return reinterpret_cast<void*>(aligned);
}

/**
 * @internal
 * @brief Adjust an allocation size request to be aligned to word size
 * and no smaller than internal minimum
 *
 * @param size
 * @param alignment
 * @return size_t
 */
size_t adjust_request_size(size_t size, size_t alignment)
{
    size_t adjusted = 0;
    if (size)
    {
        const size_t aligned = align_up(size, alignment);

        // Aligned sized must not exceed block_size_max or we'll go out of bounds on sl_bitmap
        if (aligned < k_block_size_max)
            adjusted = std::max(aligned, k_block_size_min);
    }
    return adjusted;
}

// * TLSF utils

void mapping_insert(size_t size, int& fli, int& sli)
{
    if (size < k_small_block_size)
    {
        // Store small blocks in first list
        fli = 0;
        sli = int(size) / int(k_small_block_size / k_sl_index_count);
    }
    else
    {
        fli = bit::fls_size_t(size);
        sli = (int(size) >> (fli - int(k_sl_index_count_log2))) ^ int(1 << k_sl_index_count_log2);
        fli -= (k_fl_index_shift - 1);
    }
}

// This version rounds up to the next block size (for allocations)
void mapping_search(size_t size, int& fli, int& sli)
{
    if (size >= k_small_block_size)
    {
        const size_t round = (1 << (size_t(bit::fls_size_t(size)) - k_sl_index_count_log2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}

struct TLSFAllocator::Control
{
    // Empty lists point at this block to indicate they are free
    BlockHeader null_block;

    // Bitmaps for free lists
    uint32_t fl_bitmap;                   // First Level Bitmap
    uint32_t sl_bitmap[k_fl_index_count]; // Second Level Bitmaps

    // Head of free lists
    BlockHeader* blocks[k_fl_index_count][k_sl_index_count];
};

} // namespace kb::memory