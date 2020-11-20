#pragma once

#include <cstdint>
#include <utility>
#include <ostream>

#include <kibble/memory/free_list.h>

namespace kb
{
namespace memory
{

class HeapArea;
class PoolAllocator
{
public:
	PoolAllocator() = default;
	PoolAllocator(HeapArea& area, std::size_t node_size, std::size_t max_nodes, const char* debug_name);

	void init(HeapArea& area, std::size_t node_size, std::size_t max_nodes, const char* debug_name);

	inline uint8_t* begin()             { return begin_; }
	inline const uint8_t* begin() const { return begin_; }
	inline uint8_t* end()               { return end_; }
	inline const uint8_t* end() const   { return end_; }

	void* allocate(std::size_t size, std::size_t alignment=0, std::size_t offset=0);
	void deallocate(void* ptr);
	void reset();

private:
	std::size_t node_size_;
	std::size_t max_nodes_;
	uint8_t* begin_;
	uint8_t* end_;
	Freelist free_list_;
};


} // namespace memory
} // namespace kb