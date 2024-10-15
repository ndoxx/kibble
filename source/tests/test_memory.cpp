#include "fmt/core.h"
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "kibble/memory/allocator/linear_allocator.h"
#include "kibble/memory/allocator/pool_allocator.h"
#include "kibble/memory/allocator/tlsf/impl/bit.h"
#include "kibble/memory/allocator/tlsf_allocator.h"
#include "kibble/memory/arena.h"
#include "kibble/memory/heap_area.h"
#include "kibble/memory/policy/bounds_checking_simple.h"
#include "kibble/memory/policy/memory_tracking_simple.h"
#include "kibble/memory/policy/memory_tracking_verbose.h"
#include "kibble/memory/util/literals.h"
#include "kibble/string/string.h"

#include <catch2/catch_all.hpp>

using namespace kb;
using namespace kb::memory::literals;

// 24B trivial standard layout struct
struct POD
{
    uint32_t a;
    // 4 bytes padding here for alignment of b
    uint64_t b;
    uint8_t c;
    // 7 bytes padding here for alignment of struct
};

struct NonPOD
{
    NonPOD() = default;

    NonPOD(size_t* ctor_calls_, size_t* dtor_calls_, uint32_t a, uint32_t b)
        : ctor_calls(ctor_calls_), dtor_calls(dtor_calls_), a(a), b(b), c(0x42424242)
    {
        ++(*ctor_calls);
        data = new uint32_t[a];
        for (uint32_t ii = 0; ii < a; ++ii)
        {
            data[ii] = b;
        }
    }

    ~NonPOD()
    {
        ++(*dtor_calls);
        delete[] data;
    }

    size_t* ctor_calls;
    size_t* dtor_calls;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t* data;
};

template <typename T>
void check_arrays_equal(T* array1, T* array2, size_t N)
{
    std::vector<T> vec1(array1, array1 + N);
    std::vector<T> vec2(array2, array2 + N);

    REQUIRE(vec1 == vec2);
}

#ifdef K_DEBUG
using LinArena =
    memory::MemoryArena<memory::LinearAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>;
using PoolArena =
    memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>;
using TLSFArena =
    memory::MemoryArena<memory::TLSFAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>;
#else
using LinArena =
    memory::MemoryArena<memory::LinearAllocator, memory::policy::SingleThread, memory::policy::NoBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::NoMemoryTracking>;
using PoolArena =
    memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::NoBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::NoMemoryTracking>;
using TLSFArena =
    memory::MemoryArena<memory::TLSFAllocator, memory::policy::SingleThread, memory::policy::NoBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::NoMemoryTracking>;
#endif

class LinArenaFixture
{
public:
    using SIZE_TYPE = typename LinArena::SizeType;

    LinArenaFixture() : area(3_kB), arena("LinArena", area, 2_kB)
    {
    }

protected:
    memory::HeapArea area;
    LinArena arena;
    size_t ctor_calls = 0;
    size_t dtor_calls = 0;
};

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD default alignment", "[mem][lin]")
{
    POD* some_pod = K_NEW(POD, arena);
    some_pod->a = 0x42424242;
    some_pod->b = 0xD0D0DADAD0D0DADA;
    some_pod->c = 0x69;

    // Check that returned address is correctly aligned
    REQUIRE(size_t(some_pod) % alignof(POD) == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + LinArena::k_allocation_overhead);

    K_DELETE(some_pod, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD aligned", "[mem][lin]")
{
    POD* some_pod = K_NEW_ALIGN(POD, arena, 16);
    some_pod->a = 0x42424242;
    some_pod->b = 0xD0D0DADAD0D0DADA;
    some_pod->c = 0x69;

    // Check that returned address is correctly 16 bytes aligned
    REQUIRE(size_t(some_pod) % 16 == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + LinArena::k_allocation_overhead);

    K_DELETE(some_pod, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: multiple alignments test", "[mem][lin]")
{
    for (uint32_t ALIGNMENT = 8; ALIGNMENT <= 128; ALIGNMENT *= 2)
    {
        POD* some_pod = K_NEW_ALIGN(POD, arena, ALIGNMENT);
        some_pod->a = 0x42424242;
        some_pod->b = 0xD0D0DADAD0D0DADA;
        some_pod->c = 0x69;
        REQUIRE(size_t(some_pod) % ALIGNMENT == 0);
        K_DELETE(some_pod, arena);
    }
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD array default alignment", "[mem][lin]")
{
    const uint32_t N = 10;

    POD* pod_array = K_NEW_ARRAY(POD[N], arena);
    for (uint32_t ii = 0; ii < N; ++ii)
    {
        pod_array[ii].a = 0x42424242;
        pod_array[ii].b = 0xD0D0DADAD0D0DADA;
        pod_array[ii].c = 0x69;
    }

    // Check that returned address is correctly aligned
    REQUIRE(size_t(pod_array) % alignof(POD) == 0);
    // Arena should write the complete allocation size just before the user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(pod_array) - 1) == N * sizeof(POD) + LinArena::k_allocation_overhead);

    K_DELETE_ARRAY(pod_array, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD array aligned", "[mem][lin]")
{
    const uint32_t N = 10;

    POD* pod_array = K_NEW_ARRAY_ALIGN(POD[N], arena, 32);
    for (uint32_t ii = 0; ii < N; ++ii)
    {
        pod_array[ii].a = 0x42424242;
        pod_array[ii].b = 0xD0D0DADAD0D0DADA;
        pod_array[ii].c = 0x69;
    }

    // Check that returned address is correctly 32 bytes aligned
    REQUIRE(size_t(pod_array) % 32 == 0);
    // Arena should write the complete allocation size just before the user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(pod_array) - 1) == N * sizeof(POD) + LinArena::k_allocation_overhead);

    K_DELETE_ARRAY(pod_array, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD default alignment", "[mem][lin]")
{
    NonPOD* some_non_pod = K_NEW(NonPOD, arena)(&ctor_calls, &dtor_calls, 10, 8);

    // Check that the constructor has been called
    REQUIRE(ctor_calls == 1);
    // Check that returned address is correctly aligned
    REQUIRE(size_t(some_non_pod) % alignof(NonPOD) == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_non_pod) - 1) == sizeof(NonPOD) + LinArena::k_allocation_overhead);

    K_DELETE(some_non_pod, arena);
    // Check that the destructor has been called
    REQUIRE(dtor_calls == 1);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD aligned", "[mem][lin]")
{
    NonPOD* some_non_pod = K_NEW_ALIGN(NonPOD, arena, 32)(&ctor_calls, &dtor_calls, 10, 8);

    // Check that the constructor has been called
    REQUIRE(ctor_calls == 1);
    // Check that returned address is correctly 32 bytes aligned
    REQUIRE(size_t(some_non_pod) % 32 == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_non_pod) - 1) == sizeof(NonPOD) + LinArena::k_allocation_overhead);

    K_DELETE(some_non_pod, arena);
    // Check that the destructor has been called
    REQUIRE(dtor_calls == 1);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD array default alignment", "[mem][lin]")
{
    const uint32_t N = 4;

    NonPOD* non_pod_array = K_NEW_ARRAY(NonPOD[N], arena);

    for (size_t ii = 0; ii < N; ++ii)
    {
        non_pod_array[ii].dtor_calls = &dtor_calls;
    }

    // Check that returned address is correctly aligned
    REQUIRE(size_t(non_pod_array) % alignof(NonPOD) == 0);
    // Arena should write the number of instances before the returned user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 1) == N);
    // Arena should write the complete allocation size just before the number of instances
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 2) ==
            N * sizeof(NonPOD) + LinArena::k_allocation_overhead + sizeof(SIZE_TYPE));

    K_DELETE_ARRAY(non_pod_array, arena);
    // Check that the destructor has been called
    REQUIRE(dtor_calls == N);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD array aligned", "[mem][lin]")
{
    const uint32_t N = 4;

    NonPOD* non_pod_array = K_NEW_ARRAY_ALIGN(NonPOD[N], arena, 16);

    for (size_t ii = 0; ii < N; ++ii)
    {
        non_pod_array[ii].dtor_calls = &dtor_calls;
    }

    // Check that returned address is correctly 16 bytes aligned
    REQUIRE(size_t(non_pod_array) % 16 == 0);
    // Arena should write the number of instances before the returned user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 1) == N);
    // Arena should write the complete allocation size just before the number of instances
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 2) ==
            N * sizeof(NonPOD) + LinArena::k_allocation_overhead + sizeof(SIZE_TYPE));

    K_DELETE_ARRAY(non_pod_array, arena);
    // Check that the destructor has been called
    REQUIRE(dtor_calls == N);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: multiple allocations", "[mem][lin]")
{
    for (int ii = 0; ii < 10; ++ii)
    {
        if (ii % 3)
        {
            POD* some_pod = K_NEW_ALIGN(POD, arena, 16);
            some_pod->a = 0x42424242;
            some_pod->b = 0xD0D0DADAD0D0DADA;
            some_pod->c = 0x69;
            K_DELETE(some_pod, arena);
        }
        else
        {
            NonPOD* some_non_pod = K_NEW(NonPOD, arena)(&ctor_calls, &dtor_calls, 10, 8);
            K_DELETE(some_non_pod, arena);
        }
        if (ii == 5)
        {
            POD* pod_array = K_NEW_ARRAY_ALIGN(POD[10], arena, 32);
            for (int jj = 0; jj < 10; ++jj)
            {
                pod_array[jj].a = 0x42424242;
                pod_array[jj].b = 0xD0D0DADAD0D0DADA;
                pod_array[jj].c = 0x69;
            }
            K_DELETE_ARRAY(pod_array, arena);
        }
    }

    SUCCEED();
}

class PoolArenaFixture
{
public:
    using SIZE_TYPE = typename PoolArena::SizeType;

    PoolArenaFixture() : area(3_kB), arena("PoolArena", area, 32u, sizeof(POD), 16u)
    {
    }

protected:
    memory::HeapArea area;
    PoolArena arena;
};

TEST_CASE_METHOD(PoolArenaFixture, "Pool Arena: new/delete POD default alignment", "[mem][pool]")
{
    POD* some_pod = K_NEW(POD, arena);
    some_pod->a = 0x42424242;
    some_pod->b = 0xD0D0DADAD0D0DADA;
    some_pod->c = 0x69;

    // Check that returned address is correctly aligned
    REQUIRE(size_t(some_pod) % alignof(POD) == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + PoolArena::k_allocation_overhead);

    K_DELETE(some_pod, arena);
}

TEST_CASE_METHOD(PoolArenaFixture, "Pool Arena: new/delete POD custom alignment", "[mem][pool]")
{
    POD* some_pod = K_NEW_ALIGN(POD, arena, 16);
    some_pod->a = 0x42424242;
    some_pod->b = 0xD0D0DADAD0D0DADA;
    some_pod->c = 0x69;

    // Check that returned address is correctly aligned
    REQUIRE(size_t(some_pod) % 16 == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + PoolArena::k_allocation_overhead);

    K_DELETE(some_pod, arena);
}

class TLSFArenaFixture
{
public:
    struct AllocItem
    {
        void* user_adrs;
        size_t user_size;
        size_t offset;
    };

    static constexpr size_t k_offset_single = TLSFArena::k_front_overhead;
    static constexpr size_t k_offset_array = TLSFArena::k_front_overhead + sizeof(TLSFArena::SizeType);

    TLSFArenaFixture() : area(10_kB), arena("TLSFArena", area, 2_kB)
    {
        log_walker = [](void* ptr, size_t size, bool used) {
            fmt::println("0x{:016x}> size: {}, used: {}", size_t(ptr), su::human_size(size), used);
        };
    }

    void check_integrity()
    {
        auto pool_report = arena.get_allocator().check_pool();
        auto consistency_report = arena.get_allocator().check_consistency();

        for (const auto& msg : pool_report.logs)
        {
            fmt::println("Pool> {}", msg);
        }

        for (const auto& msg : consistency_report.logs)
        {
            fmt::println("Cntl> {}", msg);
        }

        REQUIRE(pool_report.logs.empty());
        REQUIRE(consistency_report.logs.empty());
    }

    void check_allocations(const std::vector<AllocItem>& items)
    {
        std::unordered_map<size_t, size_t> alloc_size;
        for (const auto& item : items)
        {
            uint8_t* block_adrs = static_cast<uint8_t*>(item.user_adrs) - item.offset;
            // fmt::println("walk> expected block: 0x{:016x}", size_t(block_adrs));
            alloc_size.insert({size_t(block_adrs), item.user_size});
        }

        auto walker = [&alloc_size](void* ptr, size_t size, bool used) {
            // fmt::println("walk> block: 0x{:016x}", size_t(ptr));
            if (auto findit = alloc_size.find(size_t(ptr)); findit != alloc_size.end())
            {
                REQUIRE(used);
                REQUIRE(size >= findit->second);
            }
            else
            {
                REQUIRE(!used);
            }
        };

        arena.get_allocator().walk_pool(walker);
    }

    inline void display_pool() const
    {
        arena.get_allocator().walk_pool(log_walker);
    }

protected:
    memory::HeapArea area;
    TLSFArena arena;
    memory::TLSFAllocator::PoolWalker log_walker;
    size_t ctor_calls{0};
    size_t dtor_calls{0};
};

TEST_CASE("ffs", "[mem]")
{
    REQUIRE(memory::tlsf::ffs(0) == -1);
    REQUIRE(memory::tlsf::ffs(1) == 0);
    REQUIRE(memory::tlsf::ffs(int(0x80000000)) == 31);
    REQUIRE(memory::tlsf::ffs(int(0x80008000)) == 15);
}

TEST_CASE("fls", "[mem]")
{
    REQUIRE(memory::tlsf::fls(0) == -1);
    REQUIRE(memory::tlsf::fls(1) == 0);
    REQUIRE(memory::tlsf::fls(int(0x7FFFFFFF)) == 30);
    REQUIRE(memory::tlsf::fls(int(0x80000008)) == 31);
    REQUIRE(memory::tlsf::fls_size_t(0x80000000) == 31);
    REQUIRE(memory::tlsf::fls_size_t(0x100000000) == 32);
    REQUIRE(memory::tlsf::fls_size_t(0xffffffffffffffff) == 63);
}

TEST_CASE_METHOD(TLSFArenaFixture, "loadless integrity check", "[mem][tlsf]")
{
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "single POD allocation / deallocation", "[mem][tlsf]")
{
    POD* some_pod = K_NEW(POD, arena);
    check_integrity();

    // display_pool();
    check_allocations({{some_pod, sizeof(POD), k_offset_single}});

    // Check alignment
    REQUIRE(size_t(some_pod) % alignof(POD) == 0);

    K_DELETE(some_pod, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "multiple allocations / deallocations", "[mem][tlsf]")
{
    uint32_t* some_int = K_NEW(uint32_t, arena);
    POD* some_pod = K_NEW(POD, arena);
    check_integrity();

    // display_pool();
    check_allocations({{some_int, sizeof(uint32_t), k_offset_single}, {some_pod, sizeof(POD), k_offset_single}});

    // Check alignment
    REQUIRE(size_t(some_int) % 8 == 0);
    REQUIRE(size_t(some_pod) % alignof(POD) == 0);

    K_DELETE(some_int, arena);
    K_DELETE(some_pod, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "multiple POD allocation / deallocation", "[mem][tlsf]")
{
    POD* p1 = K_NEW(POD, arena);
    POD* p2 = K_NEW(POD, arena);
    POD* p3 = K_NEW(POD, arena);
    check_integrity();

    // display_pool();
    check_allocations({
        {p1, sizeof(POD), k_offset_single},
        {p2, sizeof(POD), k_offset_single},
        {p3, sizeof(POD), k_offset_single},
    });

    K_DELETE(p1, arena);
    K_DELETE(p2, arena);
    check_integrity();

    // display_pool();
    check_allocations({
        {p3, sizeof(POD), k_offset_single},
    });
}

TEST_CASE_METHOD(TLSFArenaFixture, "fragmentation and defragmentation", "[mem][tlsf]")
{
    std::vector<POD*> ptrs;
    constexpr size_t k_num_allocs = 10;

    // Allocate
    for (size_t ii = 0; ii < k_num_allocs; ++ii)
    {
        ptrs.push_back(K_NEW(POD, arena));
    }
    check_integrity();

    // Free every other allocation
    for (size_t ii = 0; ii < k_num_allocs; ii += 2)
    {
        K_DELETE(ptrs[ii], arena);
        ptrs[ii] = nullptr;
    }
    check_integrity();

    // Allocate again with larger size
    struct LargePOD
    {
        POD data[2];
    };

    for (size_t ii = 0; ii < k_num_allocs; ii += 2)
    {
        LargePOD* ptr = K_NEW(LargePOD, arena);
        REQUIRE(ptr != nullptr);
        ptrs[ii] = reinterpret_cast<POD*>(ptr);
    }
    check_integrity();

    // Free everything
    for (size_t ii = 0; ii < ptrs.size(); ++ii)
    {
        if (ptrs[ii])
        {
            if (ii % 2 == 0)
            {
                K_DELETE(reinterpret_cast<LargePOD*>(ptrs[ii]), arena);
            }
            else
            {
                K_DELETE(ptrs[ii], arena);
            }
        }
    }
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "single POD aligned allocation / deallocation", "[mem][tlsf][align]")
{
    constexpr size_t k_align = 64;
    POD* some_pod = K_NEW_ALIGN(POD, arena, k_align);
    check_integrity();

    // display_pool();
    check_allocations({{some_pod, sizeof(POD), k_offset_single}});

    // fmt::println("adrs: 0x{:016x}", size_t(some_pod));
    REQUIRE(size_t(some_pod) % k_align == 0);

    K_DELETE(some_pod, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "single POD aligned allocation / deallocation, small gap", "[mem][tlsf][align]")
{
    // This creates a small gap condition in the pool
    [[maybe_unused]] uint32_t* p1 = K_NEW(uint32_t, arena);
    [[maybe_unused]] uint32_t* p2 = K_NEW(uint32_t, arena);
    K_DELETE(p2, arena);

    constexpr size_t k_align = 64;
    POD* some_pod = K_NEW_ALIGN(POD, arena, k_align);
    check_integrity();

    // display_pool();
    check_allocations({
        {some_pod, sizeof(POD), k_offset_single},
        {p1, sizeof(uint32_t), k_offset_single},
    });

    // Check alignment
    // fmt::println("adrs: 0x{:016x}", size_t(some_pod));
    REQUIRE(size_t(some_pod) % k_align == 0);

    K_DELETE(some_pod, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "multiple aligned allocations with different alignments", "[mem][tlsf][align]")
{
    constexpr size_t k_align_16 = 16;
    constexpr size_t k_align_32 = 32;
    constexpr size_t k_align_64 = 64;
    constexpr size_t k_align_128 = 128;

    POD* pod_16 = K_NEW_ALIGN(POD, arena, k_align_16);
    POD* pod_32 = K_NEW_ALIGN(POD, arena, k_align_32);
    POD* pod_64 = K_NEW_ALIGN(POD, arena, k_align_64);
    POD* pod_128 = K_NEW_ALIGN(POD, arena, k_align_128);

    check_integrity();
    check_allocations({{pod_16, sizeof(POD), k_offset_single},
                       {pod_32, sizeof(POD), k_offset_single},
                       {pod_64, sizeof(POD), k_offset_single},
                       {pod_128, sizeof(POD), k_offset_single}});

    REQUIRE(reinterpret_cast<std::uintptr_t>(pod_16) % k_align_16 == 0);
    REQUIRE(reinterpret_cast<std::uintptr_t>(pod_32) % k_align_32 == 0);
    REQUIRE(reinterpret_cast<std::uintptr_t>(pod_64) % k_align_64 == 0);
    REQUIRE(reinterpret_cast<std::uintptr_t>(pod_128) % k_align_128 == 0);

    K_DELETE(pod_16, arena);
    K_DELETE(pod_32, arena);
    K_DELETE(pod_64, arena);
    K_DELETE(pod_128, arena);

    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "aligned allocations with odd sizes", "[mem][tlsf][align]")
{
    constexpr size_t k_align = 64;
    constexpr size_t k_size_1 = 17;  // Prime number
    constexpr size_t k_size_2 = 101; // Another prime number

    struct OddSized1
    {
        char data[k_size_1];
    };
    struct OddSized2
    {
        char data[k_size_2];
    };

    OddSized1* odd_1 = K_NEW_ALIGN(OddSized1, arena, k_align);
    OddSized2* odd_2 = K_NEW_ALIGN(OddSized2, arena, k_align);

    check_integrity();
    check_allocations({{odd_1, sizeof(OddSized1), k_offset_single}, {odd_2, sizeof(OddSized2), k_offset_single}});

    REQUIRE(reinterpret_cast<std::uintptr_t>(odd_1) % k_align == 0);
    REQUIRE(reinterpret_cast<std::uintptr_t>(odd_2) % k_align == 0);

    K_DELETE(odd_1, arena);
    K_DELETE(odd_2, arena);

    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "aligned array allocation", "[mem][tlsf][align]")
{
    constexpr size_t k_align = 128;
    constexpr size_t k_count = 10;

    POD* pod_array = K_NEW_ARRAY_ALIGN(POD[k_count], arena, k_align);
    check_integrity();
    check_allocations({{pod_array, k_count * sizeof(POD), k_offset_single}});

    REQUIRE(reinterpret_cast<std::uintptr_t>(pod_array) % k_align == 0);

    K_DELETE_ARRAY(pod_array, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "mixed allocations and deallocations", "[mem][tlsf][align]")
{
    std::vector<std::pair<void*, size_t>> allocations;
    constexpr size_t k_num_ops = 10;
    constexpr size_t k_max_size = 128;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> size_dis(1, k_max_size);
    std::uniform_int_distribution<size_t> align_dis(0, 7); // 2^0 to 2^7 alignments

    for (size_t ii = 0; ii < k_num_ops; ++ii)
    {
        if (allocations.empty() || gen() % 2 == 0)
        {
            // Allocate
            size_t size = size_dis(gen);
            size_t align = size_t{1} << align_dis(gen); // Ensure alignment is a power of 2
            void* ptr = arena.allocate(size, align, 0, __FILE__, __LINE__);
            REQUIRE(ptr != nullptr);
            REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % align == 0);
            allocations.push_back({ptr, size});
        }
        else
        {
            // Deallocate
            size_t index = gen() % allocations.size();
            arena.deallocate(allocations[index].first, nullptr, 0);
            allocations.erase(allocations.begin() + long(index));
        }

        check_integrity();
    }

    // Clean up remaining allocations
    for (const auto& alloc : allocations)
    {
        arena.deallocate(alloc.first, nullptr, 0);
    }
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "POD array allocation / deallocation", "[mem][tlsf]")
{
    constexpr size_t N = 16;
    POD* pod_array = K_NEW_ARRAY(POD[N], arena);
    check_integrity();

    // display_pool();
    check_allocations({{pod_array, N * sizeof(POD), k_offset_single}});

    K_DELETE_ARRAY(pod_array, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "byte array reallocation - next block is free", "[mem][tlsf]")
{
    auto& allocator = arena.get_allocator();

    constexpr size_t N1 = 16;
    constexpr size_t N2 = 128;

    std::byte* data = static_cast<std::byte*>(allocator.allocate(N1, alignof(std::byte), 0));
    check_integrity();

    // display_pool();
    check_allocations({{data, N1 * sizeof(std::byte), 0}});

    for (size_t ii = 0; ii < N1; ++ii)
    {
        data[ii] = std::byte(ii);
    }

    data = static_cast<std::byte*>(allocator.reallocate(data, N2, alignof(std::byte), 0));
    // display_pool();
    check_allocations({{data, N2 * sizeof(std::byte), 0}});

    for (size_t ii = N1; ii < N2; ++ii)
    {
        data[ii] = std::byte(ii);
    }

    // check data
    std::byte expected[N2];
    for (size_t ii = 0; ii < N2; ++ii)
    {
        expected[ii] = std::byte(ii);
    }
    check_arrays_equal(data, expected, N2);

    allocator.deallocate(data);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "byte array reallocation - next block is used", "[mem][tlsf]")
{
    auto& allocator = arena.get_allocator();

    constexpr size_t N1 = 16;
    constexpr size_t N2 = 128;

    std::byte* data = static_cast<std::byte*>(allocator.allocate(N1, alignof(std::byte), 0));
    POD* pod = K_NEW(POD, arena);
    check_integrity();

    // display_pool();
    check_allocations({{data, N1 * sizeof(std::byte), 0}, {pod, sizeof(POD), k_offset_single}});

    for (size_t ii = 0; ii < N1; ++ii)
    {
        data[ii] = std::byte(ii);
    }

    data = static_cast<std::byte*>(allocator.reallocate(data, N2, alignof(std::byte), 0));
    // display_pool();
    check_allocations({{data, N2 * sizeof(std::byte), 0}, {pod, sizeof(POD), k_offset_single}});

    for (size_t ii = N1; ii < N2; ++ii)
    {
        data[ii] = std::byte(ii);
    }

    // check data
    std::byte expected[N2];
    for (size_t ii = 0; ii < N2; ++ii)
    {
        expected[ii] = std::byte(ii);
    }
    check_arrays_equal(data, expected, N2);

    allocator.deallocate(data);
    K_DELETE(pod, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "single non-POD allocation / deallocation", "[mem][tlsf]")
{
    NonPOD* some_non_pod = K_NEW(NonPOD, arena)(&ctor_calls, &dtor_calls, 10, 8);
    check_integrity();

    // display_pool();
    check_allocations({{some_non_pod, sizeof(NonPOD), k_offset_single}});

    K_DELETE(some_non_pod, arena);
    check_integrity();

    REQUIRE(ctor_calls == 1);
    REQUIRE(dtor_calls == 1);
}

TEST_CASE_METHOD(TLSFArenaFixture, "non-POD array allocation / deallocation", "[mem][tlsf]")
{
    constexpr size_t N = 16;
    NonPOD* non_pod_array = K_NEW_ARRAY(NonPOD[N], arena);
    for (size_t ii = 0; ii < N; ++ii)
    {
        non_pod_array[ii].dtor_calls = &dtor_calls;
    }

    check_integrity();

    // display_pool();
    // Non-trivial types need to store the instance number before the first element
    // This is accounted for in k_offset_array
    check_allocations({{non_pod_array, N * sizeof(POD), k_offset_array}});

    K_DELETE_ARRAY(non_pod_array, arena);
    check_integrity();

    REQUIRE(dtor_calls == N);
}