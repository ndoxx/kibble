#include "memory/heap_area.h"
#include "memory/memory_utils.h"
#include "string/string.h"
#include "assert/assert.h"
#include "logger/logger.h"

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

	static const float R1 = 204.f; static const float R2 = 255.f;
	static const float G1 = 255.f; static const float G2 = 51.f;
	static const float B1 = 153.f; static const float B2 = 0.f;

	uint8_t R = uint8_t((1.f-usage)*R1 + usage*R2);
	uint8_t G = uint8_t((1.f-usage)*G1 + usage*G2);
	uint8_t B = uint8_t((1.f-usage)*B1 + usage*B2);

	KLOG("memory",1) << "Usage: " << utils::human_size(used_mem) << " / "
					 << utils::human_size(size_) << " (" 
					 << kb::WCC(R,G,B) << 100*usage << kb::WCC(0) << "%%)" << std::endl;
	for(auto&& item: items_)
	{
		usage = float(item.size) / float(used_mem);
		R = uint8_t((1.f-usage)*R1 + usage*R2);
		G = uint8_t((1.f-usage)*G1 + usage*G2);
		B = uint8_t((1.f-usage)*B1 + usage*B2);

		std::string name(item.name);
		kb::su::center(name,22);
		KLOGR("memory") << "    0x" << std::hex << item.begin << " [" << kb::WCC(R,G,B) << name << kb::WCC(0) << "] 0x" << item.end 
						<< " s=" << std::dec << utils::human_size(item.size) << std::endl;
	}
}

void HeapArea::debug_hex_dump(std::ostream& stream, size_t size)
{
    if(size == 0)
        size = size_t(head_ - begin_);
    memory::hex_dump(stream, begin_, size, "HEX DUMP");
}

bool HeapArea::init(size_t size)
{
    KLOG("memory", 1) << kb::WCC('i') << "[HeapArea]" << kb::WCC(0) << " Initializing:" << std::endl;
    size_ = size;
    begin_ = new uint8_t[size_];
    head_ = begin_;
#ifdef HEAP_AREA_MEMSET_ENABLED
    memset(begin_, AREA_MEMSET_VALUE, size_);
#endif
    KLOGI << "Size:  " << kb::WCC('v') << size_ << kb::WCC(0) << "B" << std::endl;
    KLOGI << "Begin: 0x" << std::hex << uint64_t(begin_) << std::dec << std::endl;
    return true;
}

std::pair<void*, void*> HeapArea::require_block(size_t size, const char* debug_name)
{
    // Page align returned block to avoid false sharing if multiple threads access this area
    size_t padding = utils::alignment_padding(head_, 64);
    K_ASSERT(head_ + size + padding < end(), "[HeapArea] Out of memory!");

    // Mark padding area
#ifdef HEAP_AREA_PADDING_MAGIC
    std::fill(head_, head_ + padding, AREA_PADDING_MARK);
#endif

    std::pair<void*, void*> ptr_range = {head_ + padding, head_ + padding + size + 1};

    KLOG("memory", 1) << kb::WCC('i') << "[HeapArea]" << kb::WCC(0) << " allocated aligned block:" << std::endl;
    if(debug_name)
    {
        KLOGI << "Name:      " << kb::WCC('n') << debug_name << std::endl;
    }
    KLOGI << "Size:      " << kb::WCC('v') << size << kb::WCC(0) << "B" << std::endl;
    KLOGI << "Padding:   " << kb::WCC('v') << padding << kb::WCC(0) << "B" << std::endl;
    KLOGI << "Remaining: " << kb::WCC('v')
          << static_cast<uint64_t>(static_cast<uint8_t*>(end()) - (head_ + size + padding)) << kb::WCC(0) << "B"
          << std::endl;
    KLOGI << "Address:   0x" << std::hex << reinterpret_cast<uint64_t>(head_ + padding) << std::dec << std::endl;

    head_ += size + padding;

    items_.push_back({debug_name ? debug_name : "block", ptr_range.first, ptr_range.second, size + padding});
    return ptr_range;
}

void* HeapArea::require_pool_block(size_t element_size, size_t max_count, const char* debug_name)
{
    size_t pool_size = max_count * element_size;
    auto block = require_block(pool_size, debug_name);
    return block.first;
}

} // namespace memory
} // namespace kb