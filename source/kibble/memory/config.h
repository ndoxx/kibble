#pragma once

#include <cstdint>

namespace kb::memory::cfg
{

#ifdef K_USE_MEM_AREA_MEMSET
constexpr uint8_t k_area_memset_byte = 0x00;
#endif

#ifdef K_USE_MEM_MARK_PADDING
constexpr uint8_t k_alignment_padding_mark = 0xd0;
#endif

} // namespace kb::memory::cfg