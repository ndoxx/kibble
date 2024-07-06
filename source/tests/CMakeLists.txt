# -------- UNIT TESTS -------- #

# Function to generate a test target
function(make_unit_test test_name)
    set(NAME "${test_name}")

    add_executable(${NAME}
        "${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cpp"
        catch_app.cpp
    )

    target_include_directories(${NAME}
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${KB_SOURCE_DIR}/source/kibble"
    )

    set_target_properties(${NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test"
        OUTPUT_NAME "${test_name}"
        EXCLUDE_FROM_ALL 1
    )

    target_link_libraries(${NAME}
        PRIVATE
        project_options
        project_warnings
        Catch2
        pthread
        stdc++fs
        kibble
    )
endfunction()

if(KB_TARGET_TESTS)
    # Global "tests" target to build all unit tests
    add_custom_target(tests)

    # For all cpp file beginning by "test_" generate a unit test target
    file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

    foreach(FILE ${TEST_SOURCES})
        # Extract file name without extension and use this to declare a new target
        get_filename_component(TEST_NAME ${FILE} NAME_WE)

        if(TEST_NAME MATCHES "^test_*")
            make_unit_test(${TEST_NAME})

            # Add dependency to the group target tests,
            # then we can do "make tests" to build all of them
            add_dependencies(tests "${TEST_NAME}")
            message(STATUS "Added unit test: ${TEST_NAME}")
        endif()
    endforeach()

    message(STATUS "All unit tests can be built at once with 'make tests'")

    # -------- BENCHMARKING -------- #
    # Function to generate a bench target
    function(make_benchmark test_name)
        set(NAME "${test_name}")

        add_executable(${NAME}
            "${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cpp"
        )

        target_include_directories(${NAME}
            PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${KB_SOURCE_DIR}/source/kibble"
        )

        set_target_properties(${NAME}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test"
            OUTPUT_NAME "${test_name}"
            EXCLUDE_FROM_ALL 1
        )

        target_link_libraries(${NAME}
            PRIVATE
            project_options
            stdc++fs
            kibble
            benchmark
            pthread
        )
    endfunction()

    # Global "bench" target to build all benchmarks
    add_custom_target(bench)

    # For all cpp file beginning by "test_" generate a unit test target
    file(GLOB BENCH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

    foreach(FILE ${BENCH_SOURCES})
        # Extract file name without extension and use this to declare a new target
        get_filename_component(BENCH_NAME ${FILE} NAME_WE)

        if(BENCH_NAME MATCHES "^bench_*")
            make_benchmark(${BENCH_NAME})

            # Add dependency to the group target bench,
            # then we can do "make bench" to build all of them
            add_dependencies(bench "${BENCH_NAME}")
            message(STATUS "Added benchmark: ${BENCH_NAME}")
        endif()
    endforeach()

    message(STATUS "All benchmarks can be built at once with 'make bench'")

    # -------- FUZZ TESTING -------- #
    add_executable(fuzz_tester fuzz_tester.cpp)

    target_include_directories(fuzz_tester
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${KB_SOURCE_DIR}/source/kibble"
    )

    set_target_properties(fuzz_tester
        PROPERTIES
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

    # 10 seconds fuzz testing
    # set(FUZZ_RUNTIME
    # 10
    # CACHE STRING "Number of seconds to run fuzz tests during ctest run"
    # )

    # add_test(NAME fuzz_tester_run COMMAND fuzz_tester -max_total_time=${FUZZ_RUNTIME})

    # -------- NUCLEAR TEST -------- #
    set(SRC_NUCLEAR
        nuclear.cpp
    )

    set_source_files_properties("nuclear.cpp"
        PROPERTIES COMPILE_FLAGS "-mbmi -mlzcnt"
    )

    add_executable(nuclear ${SRC_NUCLEAR})

    target_include_directories(nuclear
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${KB_SOURCE_DIR}/source/kibble"
    )

    target_include_directories(nuclear
        SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/atomic_queue/include"
    )

    set_target_properties(nuclear
        PROPERTIES
        EXCLUDE_FROM_ALL 1
    )

    target_link_libraries(nuclear
        PRIVATE
        project_options
        project_warnings
        pthread
        stdc++fs
        X11
        fmt
        kibble
    )
endif()