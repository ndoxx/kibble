#pragma once

#include <cstdint>

namespace kb
{
namespace memory
{

#pragma pack(push)
#pragma pack(1) // Force 1-byte alignment to avoid undefined behavior during (UGLY) union-cast in init()
class Freelist
{
public:
	Freelist() = default;
	Freelist(void* begin, std::size_t element_size, std::size_t max_elements, std::size_t alignment, std::size_t offset);

	void init(void* begin, std::size_t element_size, std::size_t max_elements, std::size_t alignment, std::size_t offset);

	inline void* acquire()
	{
		// Return null if no more entry left
		if(next_ == nullptr)
			return nullptr;

		// Obtain one element from the head of the free list
		Freelist* head = next_;
		next_ = head->next_;
		return head;
	}

	inline void release(void* ptr)
	{
		// Put the returned element at the head of the free list
		Freelist* head = static_cast<Freelist*>(ptr);
		head->next_ = next_;
		next_ = head;
	}
 
#ifdef K_DEBUG
	inline void* next(void* ptr)
	{
		return static_cast<Freelist*>(ptr)->next_;
	}
#endif
 
private:
  Freelist* next_;
};
#pragma pack(pop)

} // namespace memory
} // namespace kb