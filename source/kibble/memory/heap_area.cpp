#include "memory/heap_area.h"
#include "assert/assert.h"
#include "memory/util/arithmetic.h"
#include "memory/util/debug.h"
#include "string/string.h"

#include "fmt/color.h"
#include "logger2/logger.h"

// Useful to avoid uninitialized reads with Valgrind during hexdumps
// Disable for retail build
#ifdef K_ENABLE_AREA_MEMORY_INITIALIZATION
#define HEAP_AREA_MEMSET_ENABLED
#define HEAP_AREA_PADDING_MAGIC
#define AREA_MEMSET_VALUE 0x00
#define AREA_PADDING_MARK 0xd0
#endif

namespace kb
{
namespace memory
{

void HeapArea::debug_show_content()
{
    size_t b_addr = reinterpret_cast<size_t>(begin_);
    size_t h_addr = reinterpret_cast<size_t>(head_);
    size_t used_mem = h_addr - b_addr;
    float usage = float(used_mem) / float(size_);

    static const float R1 = 204.f;
    static const float R2 = 255.f;
    static const float G1 = 255.f;
    static const float G2 = 51.f;
    static const float B1 = 153.f;
    static const float B2 = 0.f;

    uint8_t R = uint8_t((1.f - usage) * R1 + usage * R2);
    uint8_t G = uint8_t((1.f - usage) * G1 + usage * G2);
    uint8_t B = uint8_t((1.f - usage) * B1 + usage * B2);

    klog(log_channel_)
        .uid("HeapArea")
        .debug("Usage: {} / {} ({}%)", utils::human_size(used_mem), utils::human_size(size_),
               fmt::styled(100 * usage, fmt::fg(fmt::rgb{R, G, B})));

    for (auto&& item : items_)
    {
        usage = float(item.size) / float(used_mem);
        R = uint8_t((1.f - usage) * R1 + usage * R2);
        G = uint8_t((1.f - usage) * G1 + usage * G2);
        B = uint8_t((1.f - usage) * B1 + usage * B2);

        std::string name(item.name);
        kb::su::center(name, 22);

        klog(log_channel_)
            .raw()
            .debug("    {:#x} [{}] {:#x} s={}", reinterpret_cast<size_t>(item.begin),
                   fmt::styled(name, fmt::fg(fmt::rgb{R, G, B})), reinterpret_cast<size_t>(item.end),
                   utils::human_size(item.size));
    }
}

HeapArea::HeapArea(size_t size, const kb::log::Channel* channel) : size_(size), log_channel_(channel)
{
    begin_ = new uint8_t[size_];
    head_ = begin_;
#ifdef HEAP_AREA_MEMSET_ENABLED
    memset(begin_, AREA_MEMSET_VALUE, size_);
#endif
    klog(log_channel_).uid("HeapArea").debug("Size: {} Begin: {:#x}", utils::human_size(size_), uint64_t(begin_));
}

HeapArea::~HeapArea()
{
    delete[] begin_;
}

void HeapArea::debug_hex_dump(size_t size)
{
    if (size == 0)
        size = size_t(head_ - begin_);
    memory::util::hex_dump(begin_, size, "HEX DUMP");
}

std::pair<void*, void*> HeapArea::require_block(size_t size, const char* debug_name)
{
    // Page align returned block to avoid false sharing if multiple threads access this area
    size_t padding = utils::alignment_padding(head_, 64);
    K_ASSERT(head_ + size + padding < end(), "[HeapArea] Out of memory!", log_channel_);

    // Mark padding area
#ifdef HEAP_AREA_PADDING_MAGIC
    std::fill(head_, head_ + padding, AREA_PADDING_MARK);
#endif

    std::pair<void*, void*> ptr_range = {head_ + padding, head_ + padding + size + 1};

    head_ += size + padding;

    items_.push_back({debug_name ? debug_name : "block", ptr_range.first, ptr_range.second, size + padding});

    klog(log_channel_)
        .uid("HeapArea")
        .debug(
            R"(allocated aligned block:
Name:      {}
Size:      {}
Padding:   {}
Remaining: {}
Address:   {:#x})",
            (debug_name ? debug_name : "ANON"), utils::human_size(size), utils::human_size(padding),
            utils::human_size(static_cast<uint64_t>(static_cast<uint8_t*>(end()) - head_)),
            reinterpret_cast<uint64_t>(head_ + padding));

    return ptr_range;
}

} // namespace memory
} // namespace kb