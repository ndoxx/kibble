#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <type_traits>
#include <vector>

#include "assert/assert.h"
#include "memory/allocator/linear_allocator.h"
#include "memory/allocator/pool_allocator.h"
#include "memory/arena.h"
#include "memory/heap_area.h"
#include "memory/linear_buffer.h"
#include "memory/policy/bounds_checking_simple.h"
#include "memory/policy/memory_tracking_simple.h"
#include "memory/util/literals.h"

#include <catch2/catch_all.hpp>

using namespace kb;
using namespace kb::memory::literals;

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
#else
using LinArena =
    memory::MemoryArena<memory::LinearAllocator, memory::policy::SingleThread, memory::policy::NoBoundsChecking,
                        memory::policy::NoMemoryTagging, memory::policy::NoMemoryTracking>;
using PoolArena =
    memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::NoBoundsChecking,
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
    for (uint32_t ALIGNMENT = 2; ALIGNMENT <= 128; ALIGNMENT *= 2)
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
