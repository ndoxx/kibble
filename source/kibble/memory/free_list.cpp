#include "memory/free_list.h"

namespace kb
{
namespace memory
{

Freelist::Freelist(void* begin, std::size_t element_size, std::size_t max_elements, std::size_t alignment, std::size_t offset)
{
	init(begin, element_size, max_elements, alignment, offset);
}

void Freelist::init(void* begin, std::size_t element_size, std::size_t max_elements, std::size_t, std::size_t)
{
	union
	{
	  void*     as_void;
	  uint8_t*  as_byte;
	  Freelist* as_self;
	};
	 
	as_void = begin;
	next_ = as_self;
	as_byte += element_size;
	 
	// initialize the free list - make every next_ of each element point to the next element in the list
	Freelist* runner = next_;
	for(std::size_t ii=1; ii<max_elements; ++ii)
	{
		runner->next_ = as_self;
		runner = as_self;
		as_byte += element_size;
	}
	 
	runner->next_ = nullptr;
}


} // namespace memory
} // namespace kb