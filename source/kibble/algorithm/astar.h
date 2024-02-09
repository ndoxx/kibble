#pragma once

#include "../memory/memory_utils.h"
#include <cstring>
#include <memory>

namespace kb
{

constexpr inline size_t round_up_pow2(int32_t base, int32_t multiple)
{
    return size_t((base + multiple - 1) & -multiple);
}

template <typename T>
class NodePool
{
public:
    struct Element
    {
        T data;
        // Intrusive linked list
        Element* next{nullptr};
    };

    static constexpr size_t k_align = 8;
    static constexpr size_t k_node_size = round_up_pow2(sizeof(Element), k_align);
    static constexpr size_t k_default_count = 128;

    NodePool(size_t max_nodes = k_default_count)
    {
        // Allocate (aligned) block
        size_t block_size = max_nodes * k_node_size;
        block_ = new (std::align_val_t(k_align)) uint8_t[block_size];
        std::memset(block_, 0, block_size);
        head_ = reinterpret_cast<Element*>(block_);

        // Setup free list
        for (size_t ii = 0; ii < max_nodes - 1; ++ii)
        {
            // NOTE(ndx): Elements are aligned
            Element* p_elem = reinterpret_cast<Element*>(block_ + k_node_size * ii);
            p_elem->next = reinterpret_cast<Element*>(block_ + k_node_size * (ii + 1));
        }
    }

    ~NodePool()
    {
        operator delete[](block_, std::align_val_t(k_align));
    }

    T* allocate()
    {
        // Return null if no more entry left
        if (head_ == nullptr)
            return nullptr;

        // Obtain one element from the head of the free list
        Element* p_elem = head_;
        head_ = p_elem->next;
        return &p_elem->data;
    }

    void deallocate(void* ptr)
    {
        // Put the returned element at the head of the free list
        Element* p_elem = reinterpret_cast<Element*>(ptr);
        p_elem->next = head_;
        head_ = p_elem;
    }

private:
    uint8_t* block_{nullptr};
    Element* head_{nullptr};
};

} // namespace kb