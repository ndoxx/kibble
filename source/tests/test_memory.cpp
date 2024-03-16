#include "fmt/format.h"
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "assert/assert.h"
#include "memory/allocator/impl/tlsf/bit.h"
#include "memory/allocator/linear_allocator.h"
#include "memory/allocator/pool_allocator.h"
#include "memory/allocator/tlsf_allocator.h"
#include "memory/arena.h"
#include "memory/heap_area.h"
#include "memory/linear_buffer.h"
#include "memory/policy/bounds_checking_simple.h"
#include "memory/policy/memory_tracking_simple.h"
#include "memory/util/literals.h"
#include "string/string.h"

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
            data[ii] = b;
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
    using SIZE_TYPE = typename LinArena::SIZE_TYPE;

    LinArenaFixture() : area(3_kB), arena("LinArena", area, 2_kB)
    {
    }

protected:
    memory::HeapArea area;
    LinArena arena;
    size_t ctor_calls = 0;
    size_t dtor_calls = 0;
};

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD default alignment", "[mem]")
{
    POD* some_pod = K_NEW(POD, arena);
    some_pod->a = 0x42424242;
    some_pod->b = 0xD0D0DADAD0D0DADA;
    some_pod->c = 0x69;

    // Check that returned address is correctly aligned
    REQUIRE(size_t(some_pod) % alignof(POD) == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + LinArena::DECORATION_SIZE);

    K_DELETE(some_pod, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD aligned", "[mem]")
{
    POD* some_pod = K_NEW_ALIGN(POD, arena, 16);
    some_pod->a = 0x42424242;
    some_pod->b = 0xD0D0DADAD0D0DADA;
    some_pod->c = 0x69;

    // Check that returned address is correctly 16 bytes aligned
    REQUIRE(size_t(some_pod) % 16 == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + LinArena::DECORATION_SIZE);

    K_DELETE(some_pod, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: multiple alignments test", "[mem]")
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

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD array default alignment", "[mem]")
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
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(pod_array) - 1) == N * sizeof(POD) + LinArena::DECORATION_SIZE);

    K_DELETE_ARRAY(pod_array, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD array aligned", "[mem]")
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
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(pod_array) - 1) == N * sizeof(POD) + LinArena::DECORATION_SIZE);

    K_DELETE_ARRAY(pod_array, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD default alignment", "[mem]")
{
    NonPOD* some_non_pod = K_NEW(NonPOD, arena)(&ctor_calls, &dtor_calls, 10, 8);

    // Check that the constructor has been called
    REQUIRE(ctor_calls == 1);
    // Check that returned address is correctly aligned
    REQUIRE(size_t(some_non_pod) % alignof(NonPOD) == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_non_pod) - 1) == sizeof(NonPOD) + LinArena::DECORATION_SIZE);

    K_DELETE(some_non_pod, arena);
    // Check that the destructor has been called
    REQUIRE(dtor_calls == 1);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD aligned", "[mem]")
{
    NonPOD* some_non_pod = K_NEW_ALIGN(NonPOD, arena, 32)(&ctor_calls, &dtor_calls, 10, 8);

    // Check that the constructor has been called
    REQUIRE(ctor_calls == 1);
    // Check that returned address is correctly 32 bytes aligned
    REQUIRE(size_t(some_non_pod) % 32 == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_non_pod) - 1) == sizeof(NonPOD) + LinArena::DECORATION_SIZE);

    K_DELETE(some_non_pod, arena);
    // Check that the destructor has been called
    REQUIRE(dtor_calls == 1);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD array default alignment", "[mem]")
{
    const uint32_t N = 4;

    NonPOD* non_pod_array = K_NEW_ARRAY(NonPOD[N], arena);

    for (size_t ii = 0; ii < N; ++ii)
        non_pod_array[ii].dtor_calls = &dtor_calls;

    // Check that returned address is correctly aligned
    REQUIRE(size_t(non_pod_array) % alignof(NonPOD) == 0);
    // Arena should write the number of instances before the returned user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 1) == N);
    // Arena should write the complete allocation size just before the number of instances
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 2) ==
            N * sizeof(NonPOD) + LinArena::DECORATION_SIZE + sizeof(SIZE_TYPE));

    K_DELETE_ARRAY(non_pod_array, arena);
    // Check that the destructor has been called
    REQUIRE(dtor_calls == N);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD array aligned", "[mem]")
{
    const uint32_t N = 4;

    NonPOD* non_pod_array = K_NEW_ARRAY_ALIGN(NonPOD[N], arena, 16);

    for (size_t ii = 0; ii < N; ++ii)
        non_pod_array[ii].dtor_calls = &dtor_calls;

    // Check that returned address is correctly 16 bytes aligned
    REQUIRE(size_t(non_pod_array) % 16 == 0);
    // Arena should write the number of instances before the returned user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 1) == N);
    // Arena should write the complete allocation size just before the number of instances
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 2) ==
            N * sizeof(NonPOD) + LinArena::DECORATION_SIZE + sizeof(SIZE_TYPE));

    K_DELETE_ARRAY(non_pod_array, arena);
    // Check that the destructor has been called
    REQUIRE(dtor_calls == N);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: multiple allocations", "[mem]")
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
    using SIZE_TYPE = typename PoolArena::SIZE_TYPE;

    PoolArenaFixture() : area(3_kB), arena("PoolArena", area, 32u, sizeof(POD), 16u)
    {
    }

protected:
    memory::HeapArea area;
    PoolArena arena;
};

TEST_CASE_METHOD(PoolArenaFixture, "Pool Arena: new/delete POD default alignment", "[mem]")
{
    POD* some_pod = K_NEW(POD, arena);
    some_pod->a = 0x42424242;
    some_pod->b = 0xD0D0DADAD0D0DADA;
    some_pod->c = 0x69;

    // Check that returned address is correctly aligned
    REQUIRE(size_t(some_pod) % alignof(POD) == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + PoolArena::DECORATION_SIZE);

    K_DELETE(some_pod, arena);
}

TEST_CASE_METHOD(PoolArenaFixture, "Pool Arena: new/delete POD custom alignment", "[mem]")
{
    POD* some_pod = K_NEW_ALIGN(POD, arena, 16);
    some_pod->a = 0x42424242;
    some_pod->b = 0xD0D0DADAD0D0DADA;
    some_pod->c = 0x69;

    // Check that returned address is correctly aligned
    REQUIRE(size_t(some_pod) % 16 == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + PoolArena::DECORATION_SIZE);

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

    static constexpr size_t k_offset_single = TLSFArena::BK_FRONT_SIZE;
    static constexpr size_t k_offset_array = TLSFArena::BK_FRONT_SIZE + sizeof(TLSFArena::SIZE_TYPE);

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
            fmt::println("Pool> {}", msg);

        for (const auto& msg : consistency_report.logs)
            fmt::println("Cntl> {}", msg);

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

TEST_CASE_METHOD(TLSFArenaFixture, "loadless integrity check", "[mem]")
{
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "single POD allocation / deallocation", "[mem]")
{
    POD* some_pod = K_NEW(POD, arena);
    check_integrity();

    display_pool();
    fmt::println("adrs: 0x{:016x}", size_t(some_pod));
    check_allocations({{some_pod, sizeof(POD), k_offset_single}});

    // Check alignment
    // REQUIRE(size_t(some_pod) % alignof(POD) == 0);

    K_DELETE(some_pod, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "multiple POD allocation / deallocation", "[mem]")
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

TEST_CASE_METHOD(TLSFArenaFixture, "single POD aligned allocation / deallocation", "[mem]")
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

TEST_CASE_METHOD(TLSFArenaFixture, "single POD aligned allocation / deallocation, small gap", "[mem]")
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

TEST_CASE_METHOD(TLSFArenaFixture, "POD array allocation / deallocation", "[mem]")
{
    constexpr size_t N = 16;
    POD* pod_array = K_NEW_ARRAY(POD[N], arena);
    check_integrity();

    // display_pool();
    check_allocations({{pod_array, N * sizeof(POD), k_offset_single}});

    K_DELETE_ARRAY(pod_array, arena);
    check_integrity();
}

TEST_CASE_METHOD(TLSFArenaFixture, "single non-POD allocation / deallocation", "[mem]")
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

TEST_CASE_METHOD(TLSFArenaFixture, "non-POD array allocation / deallocation", "[mem]")
{
    constexpr size_t N = 16;
    NonPOD* non_pod_array = K_NEW_ARRAY(NonPOD[N], arena);
    for (size_t ii = 0; ii < N; ++ii)
        non_pod_array[ii].dtor_calls = &dtor_calls;

    check_integrity();

    // display_pool();
    // Non-trivial types need to store the instance number before the first element
    // This is accounted for in k_offset_array
    check_allocations({{non_pod_array, N * sizeof(POD), k_offset_array}});

    K_DELETE_ARRAY(non_pod_array, arena);
    check_integrity();

    REQUIRE(dtor_calls == N);
}