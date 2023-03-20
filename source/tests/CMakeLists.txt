# -------- UNIT TESTS -------- #

set(SRC_KIBBLE_TEST
    catch_app.cpp
    test_hash.cpp
    test_ap.cpp
    test_memory.cpp
    test_job.cpp
    test_fs.cpp
    test_undo.cpp
    test_event.cpp
    test_optimization.cpp
    test_settings.cpp
    test_uuid.cpp
   )

add_executable(test_kibble ${SRC_KIBBLE_TEST})

target_include_directories(test_kibble
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

target_compile_definitions(test_kibble PRIVATE -DLOGGING_ANSI_3=0)
target_compile_definitions(test_kibble PRIVATE -DLOGGING_ENABLED=1)

set_target_properties(test_kibble
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/test"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/test"
        EXCLUDE_FROM_ALL 1
)

target_link_libraries(test_kibble
    PRIVATE
        project_options
        project_warnings
        Catch2
        pthread
        stdc++fs
        kibble
)



# -------- FUZZ TESTING -------- #

add_executable(fuzz_tester fuzz_tester.cpp)

target_include_directories(fuzz_tester
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

set_target_properties(fuzz_tester
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/test"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/test"
        EXCLUDE_FROM_ALL 1
)

target_link_libraries(fuzz_tester
    PRIVATE
        project_options
        project_warnings
        kibble
        -coverage
        -fsanitize=fuzzer,undefined,address
    )

target_compile_options(fuzz_tester
    PRIVATE
        -fsanitize=fuzzer,undefined,address
    )

target_compile_definitions(fuzz_tester PRIVATE -DLOGGING_ENABLED=0)

# 10 seconds fuzz testing
# set(FUZZ_RUNTIME
#     10
#     CACHE STRING "Number of seconds to run fuzz tests during ctest run"
# )

# add_test(NAME fuzz_tester_run COMMAND fuzz_tester -max_total_time=${FUZZ_RUNTIME})



# -------- NUCLEAR TEST -------- #

set(SRC_NUCLEAR
    nuclear.cpp
   )

add_executable(nuclear ${SRC_NUCLEAR})

target_include_directories(nuclear
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

target_include_directories(nuclear
SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/atomic_queue/include"
)

target_compile_definitions(nuclear PRIVATE -DLOGGING_ANSI_3=0)
target_compile_definitions(nuclear PRIVATE -DLOGGING_ENABLED=1)

set_target_properties(nuclear
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/test"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/test"
        EXCLUDE_FROM_ALL 1
    )

target_link_libraries(nuclear
    PRIVATE
        project_options
        project_warnings
        pthread
        stdc++fs
        X11
        kibble
    )



# -------- BENCHMARKING -------- #

add_executable(bench "${CMAKE_CURRENT_SOURCE_DIR}/bench.cpp")

target_include_directories(bench
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

target_compile_definitions(bench PRIVATE -DLOGGING_ENABLED=0)

set_target_properties(bench
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/test"
        EXCLUDE_FROM_ALL 1
    )

target_link_libraries(bench
    PRIVATE
        project_options
        stdc++fs
        kibble
        benchmark
        pthread
    )
