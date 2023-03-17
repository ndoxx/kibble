#include "memory/heap_area.h"
#include "assert/assert.h"
// #include "logger/logger.h"
#include "memory/memory_utils.h"
#include "string/string.h"

#include "logger2/logger.h"
#include <fmt/color.h>

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

    klog2(log_channel_)
        .debug("Usage: {} / {} ({}%)", utils::human_size(used_mem), utils::human_size(size_),
               fmt::styled(100 * usage, fmt::fg(fmt::rgb{R, G, B})));

    // KLOG("memory", 1) << "Usage: " << utils::human_size(used_mem) << " / " << utils::human_size(size_) << " ("
    //                   << kb::KF_(R, G, B) << 100 * usage << kb::KC_ << "%%)" << std::endl;
    for (auto &&item : items_)
    {
        usage = float(item.size) / float(used_mem);
        R = uint8_t((1.f - usage) * R1 + usage * R2);
        G = uint8_t((1.f - usage) * G1 + usage * G2);
        B = uint8_t((1.f - usage) * B1 + usage * B2);

        std::string name(item.name);
        kb::su::center(name, 22);

        klog2(log_channel_)
            .raw()
            .debug("    {:#x} [{}] {:#x} s={}", reinterpret_cast<size_t>(item.begin),
                   fmt::styled(name, fmt::fg(fmt::rgb{R, G, B})), reinterpret_cast<size_t>(item.end),
                   utils::human_size(item.size));

        // KLOGR("memory") << "    0x" << std::hex << item.begin << " [" << kb::KF_(R, G, B) << name << kb::KC_ << "]0x"
        //                 << item.end << " s=" << std::dec << utils::human_size(item.size) << std::endl;
    }
}

HeapArea::HeapArea(size_t size, const kb::log::Channel *channel) : log_channel_(channel)
{
    (void)log_channel_;
    if (!init(size))
        throw std::bad_alloc();
}

HeapArea::~HeapArea()
{
    delete[] begin_;
}

void HeapArea::debug_hex_dump(std::ostream &stream, size_t size)
{
    if (size == 0)
        size = size_t(head_ - begin_);
    memory::hex_dump(stream, begin_, size, "HEX DUMP");
}

bool HeapArea::init(size_t size)
{
    // KLOG("memory", 1) << kb::KS_INST_ << "[HeapArea]" << kb::KC_ << " Initializing:" << std::endl;
    size_ = size;
    try
    {
        begin_ = new uint8_t[size_];
    }
    catch (std::bad_alloc const &)
    {
        return false;
    }
    head_ = begin_;
#ifdef HEAP_AREA_MEMSET_ENABLED
    memset(begin_, AREA_MEMSET_VALUE, size_);
#endif
    // KLOGI << "Size:  " << kb::KS_VALU_ << size_ << kb::KC_ << "B" << std::endl;
    // KLOGI << "Begin: 0x" << std::hex << uint64_t(begin_) << std::dec << std::endl;
    klog2(log_channel_).debug("[HeapArea] Size: {} Begin: {:#x}", utils::human_size(size_), uint64_t(begin_));

    return true;
}

std::pair<void *, void *> HeapArea::require_block(size_t size, const char *debug_name)
{
    // Page align returned block to avoid false sharing if multiple threads access this area
    size_t padding = utils::alignment_padding(head_, 64);
    K_ASSERT(head_ + size + padding < end(), "[HeapArea] Out of memory!");

    // Mark padding area
#ifdef HEAP_AREA_PADDING_MAGIC
    std::fill(head_, head_ + padding, AREA_PADDING_MARK);
#endif

    std::pair<void *, void *> ptr_range = {head_ + padding, head_ + padding + size + 1};

    // KLOG("memory", 1) << kb::KS_INST_ << "[HeapArea]" << kb::KC_ << " allocated aligned block:" << std::endl;
    // if (debug_name)
    // {
    //     KLOGI << "Name:      " << kb::KS_NAME_ << debug_name << std::endl;
    // }
    // KLOGI << "Size:      " << kb::KS_VALU_ << size << kb::KC_ << "B" << std::endl;
    // KLOGI << "Padding:   " << kb::KS_VALU_ << padding << kb::KC_ << "B" << std::endl;
    // KLOGI << "Remaining: " << kb::KS_VALU_
    //       << static_cast<uint64_t>(static_cast<uint8_t *>(end()) - (head_ + size + padding)) << kb::KC_ << "B"
    //       << std::endl;
    // KLOGI << "Address:   0x" << std::hex << reinterpret_cast<uint64_t>(head_ + padding) << std::dec << std::endl;

    head_ += size + padding;

    items_.push_back({debug_name ? debug_name : "block", ptr_range.first, ptr_range.second, size + padding});

    klog2(log_channel_)
        .debug(
R"([HeapArea] allocated aligned block:
Name:      {}
Size:      {}
Padding:   {}
Remaining: {}
Address:   {:#x})",
            (debug_name ? debug_name : "ANON"), utils::human_size(size), utils::human_size(padding),
            utils::human_size(static_cast<uint64_t>(static_cast<uint8_t *>(end()) - head_)),
            reinterpret_cast<uint64_t>(head_ + padding));

    return ptr_range;
}

void *HeapArea::require_pool_block(size_t element_size, size_t max_count, const char *debug_name)
{
    size_t pool_size = max_count * element_size;
    auto block = require_block(pool_size, debug_name);
    return block.first;
}

} // namespace memory
} // namespace kb