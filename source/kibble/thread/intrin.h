#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
	#include <emmintrin.h>
#else
	#error Unsupported architecture
#endif

namespace kb
{
namespace intrin
{

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
	[[maybe_unused]] static inline void spin__() noexcept { _mm_pause(); }
#else
	#error Unsupported architecture
#endif

} // namespace intrin
} // namespace kb