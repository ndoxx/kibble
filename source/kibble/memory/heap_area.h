#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <ostream>

namespace kb
{
namespace memory
{

namespace debug
{

struct AreaItem
{
    std::string name;
    void* begin = nullptr;
    void* end = nullptr;
    std::size_t size = 0;
};

} // namespace debug

class HeapArea
{
public:
    HeapArea() = default;
    explicit HeapArea(size_t size) { init(size); }

    ~HeapArea() { delete[] begin_; }

    bool init(size_t size);

    inline void* begin() { return begin_; }
    inline void* end() { return begin_ + size_ + 1; }
    inline std::pair<void*, void*> range() { return {begin(), end()}; }

    // Get a range of pointers to a memory block within area, and advance head
    std::pair<void*, void*> require_block(size_t size, const char* debug_name = nullptr);
    void* require_pool_block(size_t element_size, size_t max_count, const char* debug_name = nullptr);

    void debug_show_content();
    void debug_hex_dump(std::ostream& stream, size_t size = 0);
    inline const std::vector<debug::AreaItem>& get_block_descriptions() const { return items_; }
    inline void fill(uint8_t filler) { std::fill(begin_, begin_ + size_, filler); }

private:
    size_t size_;
    uint8_t* begin_;
    uint8_t* head_;

    std::vector<debug::AreaItem> items_; // for debug
};

} // namespace memory
} // namespace kb