#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <type_traits>
#include <vector>

#include "assert/assert.h"
#include "logger/logger.h"
#include "memory/heap_area.h"
#include "memory/linear_allocator.h"
#include "memory/memory.h"
#include "memory/memory_utils.h"
#include "memory/pool_allocator.h"
#include <catch2/catch_all.hpp>

using namespace kb;

struct POD
{
    uint32_t a;
    uint32_t b;
    uint64_t c;
};

struct NonPOD
{
    NonPOD(uint32_t a, uint32_t b) : a(a), b(b), c(0x42424242)
    {
        data = new uint32_t[a];
        for (uint32_t ii = 0; ii < a; ++ii)
            data[ii] = b;
    }

    NonPOD() : NonPOD(4, 0xB16B00B5)
    {
    }

    ~NonPOD()
    {
        delete[] data;
    }

    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t* data;
};

typedef memory::MemoryArena<memory::LinearAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                            memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>
    LinArena;

class LinArenaFixture
{
public:
    typedef typename LinArena::SIZE_TYPE SIZE_TYPE;

    LinArenaFixture() : area(3_kB), arena(area, 2_kB, "LinArena")
    {
    }

protected:
    memory::HeapArea area;
    LinArena arena;
};

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD non-aligned", "[mem]")
{
    POD* some_pod = K_NEW(POD, arena);
    some_pod->a = 0x42424242;
    some_pod->b = 0xB16B00B5;
    some_pod->c = 0xD0D0DADAD0D0DADA;
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(some_pod) - LinArena::BK_FRONT_SIZE, sizeof(POD) +
    // LinArena::DECORATION_SIZE);

    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + LinArena::DECORATION_SIZE);

    K_DELETE(some_pod, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD aligned", "[mem]")
{
    POD* some_pod = K_NEW_ALIGN(POD, arena, 16);
    some_pod->a = 0x42424242;
    some_pod->b = 0xB16B00B5;
    some_pod->c = 0xD0D0DADAD0D0DADA;
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(some_pod) - LinArena::BK_FRONT_SIZE, sizeof(POD) +
    // LinArena::DECORATION_SIZE);

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
        some_pod->a = 0xB16B00B5;
        REQUIRE(size_t(some_pod) % ALIGNMENT == 0);
        K_DELETE(some_pod, arena);
    }
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(area.begin()), 512);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new POD array non-aligned", "[mem]")
{
    const uint32_t N = 10;

    POD* pod_array = K_NEW_ARRAY(POD[N], arena);
    for (uint32_t ii = 0; ii < N; ++ii)
    {
        pod_array[ii].a = 0x42424242;
        pod_array[ii].b = 0xB16B00B5;
        pod_array[ii].c = 0xD0D0DADAD0D0DADA;
    }
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(pod_array) - LinArena::BK_FRONT_SIZE - 4, N*sizeof(POD) +
    // LinArena::DECORATION_SIZE + 4);

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
        pod_array[ii].b = 0xB16B00B5;
        pod_array[ii].c = 0xD0D0DADAD0D0DADA;
    }
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(pod_array) - LinArena::BK_FRONT_SIZE - 4, N*sizeof(POD) +
    // LinArena::DECORATION_SIZE + 4);

    // Check that returned address is correctly 32 bytes aligned
    REQUIRE(size_t(pod_array) % 32 == 0);
    // Arena should write the complete allocation size just before the user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(pod_array) - 1) == N * sizeof(POD) + LinArena::DECORATION_SIZE);

    K_DELETE_ARRAY(pod_array, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD non-aligned", "[mem]")
{
    NonPOD* some_non_pod = K_NEW(NonPOD, arena)(10, 8);
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(some_non_pod) - LinArena::BK_FRONT_SIZE, sizeof(NonPOD) +
    // LinArena::DECORATION_SIZE);

    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_non_pod) - 1) == sizeof(NonPOD) + LinArena::DECORATION_SIZE);

    K_DELETE(some_non_pod, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD aligned", "[mem]")
{
    NonPOD* some_non_pod = K_NEW_ALIGN(NonPOD, arena, 32)(10, 8);
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(some_non_pod) - LinArena::BK_FRONT_SIZE, sizeof(NonPOD) +
    // LinArena::DECORATION_SIZE);

    // Check that returned address is correctly 32 bytes aligned
    REQUIRE(size_t(some_non_pod) % 32 == 0);
    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_non_pod) - 1) == sizeof(NonPOD) + LinArena::DECORATION_SIZE);

    K_DELETE(some_non_pod, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD array non-aligned", "[mem]")
{
    const uint32_t N = 4;

    NonPOD* non_pod_array = K_NEW_ARRAY(NonPOD[N], arena);
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(non_pod_array) - LinArena::BK_FRONT_SIZE - 4,
    // N*sizeof(NonPOD) + LinArena::DECORATION_SIZE + 4);

    // Arena should write the number of instances before the returned user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 1) == N);
    // Arena should write the complete allocation size just before the number of instances
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 2) ==
            N * sizeof(NonPOD) + LinArena::DECORATION_SIZE + sizeof(SIZE_TYPE));

    K_DELETE_ARRAY(non_pod_array, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: new non-POD array aligned", "[mem]")
{
    const uint32_t N = 4;

    NonPOD* non_pod_array = K_NEW_ARRAY_ALIGN(NonPOD[N], arena, 16);
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(non_pod_array) - LinArena::BK_FRONT_SIZE - 4,
    // N*sizeof(NonPOD) + LinArena::DECORATION_SIZE + 4);

    // Check that returned address is correctly 16 bytes aligned
    REQUIRE(size_t(non_pod_array) % 16 == 0);
    // Arena should write the number of instances before the returned user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 1) == N);
    // Arena should write the complete allocation size just before the number of instances
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(non_pod_array) - 2) ==
            N * sizeof(NonPOD) + LinArena::DECORATION_SIZE + sizeof(SIZE_TYPE));

    K_DELETE_ARRAY(non_pod_array, arena);
}

TEST_CASE_METHOD(LinArenaFixture, "Linear Arena: multiple allocations", "[mem]")
{
    for (int ii = 0; ii < 10; ++ii)
    {
        if (ii % 3)
        {
            POD* some_pod = K_NEW_ALIGN(POD, arena, 16);
            some_pod->a = 0x42424242;
            some_pod->b = 0xB16B00B5;
            some_pod->c = 0xD0D0DADAD0D0DADA;
            K_DELETE(some_pod, arena);
        }
        else
        {
            NonPOD* some_non_pod = K_NEW(NonPOD, arena)(10, 8);
            K_DELETE(some_non_pod, arena);
        }
        if (ii == 5)
        {
            POD* pod_array = K_NEW_ARRAY_ALIGN(POD[10], arena, 32);
            for (int jj = 0; jj < 10; ++jj)
            {
                pod_array[jj].a = 0x42424242;
                pod_array[jj].b = 0xB16B00B5;
                pod_array[jj].c = 0xD0D0DADAD0D0DADA;
            }
            K_DELETE_ARRAY(pod_array, arena);
        }
    }
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(area.begin()), 1_kB);

    SUCCEED();
}

typedef memory::MemoryArena<memory::PoolAllocator, memory::policy::SingleThread, memory::policy::SimpleBoundsChecking,
                            memory::policy::NoMemoryTagging, memory::policy::SimpleMemoryTracking>
    PoolArena;

class PoolArenaFixture
{
public:
    typedef typename PoolArena::SIZE_TYPE SIZE_TYPE;

    PoolArenaFixture() : area(3_kB), arena(area, sizeof(POD) + PoolArena::DECORATION_SIZE, 32u, "PoolArena")
    {
    }

protected:
    memory::HeapArea area;
    PoolArena arena;
};

TEST_CASE_METHOD(PoolArenaFixture, "Pool Arena: new/delete POD non-aligned", "[mem]")
{
    POD* some_pod = K_NEW(POD, arena);
    some_pod->a = 0x42424242;
    some_pod->b = 0xB16B00B5;
    some_pod->c = 0xD0D0DADAD0D0DADA;
    // memory::hex_dump(std::cout, reinterpret_cast<uint8_t*>(some_pod) - PoolArena::BK_FRONT_SIZE, sizeof(POD) +
    // PoolArena::DECORATION_SIZE);

    // Arena should write the complete allocation size just before user pointer
    REQUIRE(*(reinterpret_cast<SIZE_TYPE*>(some_pod) - 1) == sizeof(POD) + PoolArena::DECORATION_SIZE);

    K_DELETE(some_pod, arena);
}

/* // TODO: Test pool arena aligned allocations

typedef memory::MemoryArena<memory::PoolAllocator,
                            memory::policy::SingleThread,
                            memory::policy::SimpleBoundsChecking,
                            memory::policy::NoMemoryTagging,
                            memory::policy::SimpleMemoryTracking> MyPoolArena;

// typedef memory::MemoryArena<memory::PoolAllocator,
// 		    				memory::policy::SingleThread,
// 		    				memory::policy::NoBoundsChecking,
// 		    				memory::policy::NoMemoryTagging,
// 		    				memory::policy::NoMemoryTracking> MyPoolArena;


    uint32_t dump_size = 512_B;

    uint32_t max_foo = 8;
    uint32_t max_align = 16-1;
    uint32_t node_size = sizeof(Foo) + max_align;

    memory::HeapArea area(8_kB);
    area.fill(0xaa);
    MyPoolArena pool(area.require_pool_block<MyPoolArena>(node_size, max_foo), node_size, max_foo,
MyPoolArena::DECORATION_SIZE);

    std::vector<Foo*> foos;

    area.debug_hex_dump(std::cout, dump_size);
    for(int kk=0; kk<2; ++kk)
    {
        for(int ii=0; ii<8; ++ii)
        {
            Foo* foo = K_NEW_ALIGN(Foo, pool, 8);
            foos.push_back(foo);
        }
        area.debug_hex_dump(std::cout, dump_size);

        K_DELETE(foos[2], pool); foos[2] = std::move(foos.back()); foos.pop_back();
        K_DELETE(foos[5], pool); foos[5] = std::move(foos.back()); foos.pop_back();
        foos.push_back(K_NEW_ALIGN(Foo, pool, 16));
        foos.push_back(K_NEW(Foo, pool));
        area.debug_hex_dump(std::cout, dump_size);

        for(Foo* foo: foos)
        {
            K_DELETE(foo, pool);
        }
        area.debug_hex_dump(std::cout, dump_size);

        foos.clear();
    }
*/

/*
HANDLE_DECLARATION( FooHandle , 128 );
HANDLE_DEFINITION( FooHandle );
HANDLE_DECLARATION( BarHandle , 128 );
HANDLE_DEFINITION( BarHandle );

class HandleFixture
{
public:
    HandleFixture():
    area(3_kB),
    arena(area, 2_kB, "LinearArena")
    {
        FooHandle::init_pool(arena);
        BarHandle::init_pool(arena);
    }

    ~HandleFixture()
    {
        FooHandle::destroy_pool(arena);
        BarHandle::destroy_pool(arena);
    }

protected:
    memory::HeapArea area;
    LinearArena arena;
};

TEST_CASE_METHOD(HandleFixture, "Assessing default handle properties", "[hnd]")
{
    FooHandle foo;

    REQUIRE(foo.index == k_invalid_handle);
    REQUIRE(!foo.is_valid());
}

TEST_CASE_METHOD(HandleFixture, "Acquiring a single handle of one type", "[hnd]")
{
    FooHandle foo = FooHandle::acquire();

    REQUIRE(foo.index == 0);
}

TEST_CASE_METHOD(HandleFixture, "Acquiring two consecutive handles", "[hnd]")
{
    FooHandle foo0 = FooHandle::acquire();
    FooHandle foo1 = FooHandle::acquire();

    REQUIRE(foo0.index == 0);
    REQUIRE(foo1.index == 1);
}

TEST_CASE_METHOD(HandleFixture, "Handle equal operation", "[hnd]")
{
    FooHandle foo0 = FooHandle::acquire();
    FooHandle foo1 = foo0;

    REQUIRE(foo0 == foo1);
}

TEST_CASE_METHOD(HandleFixture, "Handle not equal operation", "[hnd]")
{
    FooHandle foo0 = FooHandle::acquire();
    FooHandle foo1 = FooHandle::acquire();

    REQUIRE(foo0 != foo1);
}

TEST_CASE_METHOD(HandleFixture, "Acquiring a single handle of each type", "[hnd]")
{
    FooHandle foo = FooHandle::acquire();
    BarHandle bar = BarHandle::acquire();

    REQUIRE(foo.index == 0);
    REQUIRE(bar.index == 0);
}

TEST_CASE_METHOD(HandleFixture, "Checking that handle indices are reused", "[hnd]")
{
    FooHandle foo0 = FooHandle::acquire();
    FooHandle foo1 = FooHandle::acquire();
    FooHandle foo2 = FooHandle::acquire();
    FooHandle foo3 = FooHandle::acquire();

    foo1.release();
    foo3.release();

    REQUIRE(!foo1.is_valid());
    REQUIRE(!foo3.is_valid());

    FooHandle foo4 = FooHandle::acquire();
    FooHandle foo5 = FooHandle::acquire();

    REQUIRE(foo4.index == 3);
    REQUIRE(foo5.index == 1);
}
*/

/*
ROBUST_HANDLE_DECLARATION( BazHandle );
ROBUST_HANDLE_DEFINITION( BazHandle, 8 );
ROBUST_HANDLE_DECLARATION( QuxHandle );
ROBUST_HANDLE_DEFINITION( QuxHandle, 8 );

class RobustHandleFixture
{
public:
    RobustHandleFixture():
    area(3_kB),
    arena(area, 2_kB, "LinearArena")
    {
        BazHandle::init_pool(arena);
        QuxHandle::init_pool(arena);
    }

    ~RobustHandleFixture()
    {
        BazHandle::destroy_pool(arena);
        QuxHandle::destroy_pool(arena);
    }

protected:
    memory::HeapArea area;
    LinearArena arena;
};

TEST_CASE_METHOD(RobustHandleFixture, "RobustHandle: Assessing default handle properties", "[hnd]")
{
    BazHandle baz;

    REQUIRE(baz.index == k_invalid_robust_handle);
    REQUIRE(baz.counter == k_invalid_robust_handle);
    REQUIRE(!baz.is_valid());
}

TEST_CASE_METHOD(RobustHandleFixture, "RobustHandle: Acquiring a single handle of one type", "[hnd]")
{
    BazHandle baz = BazHandle::acquire();

    REQUIRE(baz.index == 0);
    REQUIRE(baz.counter == 0);
}

TEST_CASE_METHOD(RobustHandleFixture, "RobustHandle: Acquiring two consecutive handles", "[hnd]")
{
    BazHandle baz0 = BazHandle::acquire();
    BazHandle baz1 = BazHandle::acquire();

    REQUIRE(baz0.index == 0);
    REQUIRE(baz0.counter == 0);
    REQUIRE(baz1.index == 1);
    REQUIRE(baz1.counter == 0);
}

TEST_CASE_METHOD(RobustHandleFixture, "RobustHandle: Assessing counter", "[hnd]")
{
    BazHandle baz = BazHandle::acquire();
    BazHandle baz_copy = baz;
    baz.release();
    REQUIRE(!baz.is_valid());

    baz = BazHandle::acquire();
    REQUIRE(baz.is_valid());
    REQUIRE(!baz_copy.is_valid());
    REQUIRE(baz_copy.index == baz.index);
    REQUIRE(baz.counter > baz_copy.counter);
}

TEST_CASE_METHOD(RobustHandleFixture, "RobustHandle: Handle equal/not equal operations", "[hnd]")
{
    BazHandle baz = BazHandle::acquire();
    BazHandle baz_copy = baz;

    REQUIRE(baz_copy == baz);

    baz.release();
    baz = BazHandle::acquire();

    REQUIRE(baz_copy != baz);
}

TEST_CASE_METHOD(RobustHandleFixture, "RobustHandle: Acquiring a single handle of each type", "[hnd]")
{
    BazHandle baz = BazHandle::acquire();
    QuxHandle qux = QuxHandle::acquire();

    REQUIRE(baz.index == 0);
    REQUIRE(baz.counter == 0);
    REQUIRE(qux.index == 0);
    REQUIRE(qux.counter == 0);
}
*/