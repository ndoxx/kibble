# -------- UNIT TESTS -------- #

if(KB_TARGET_TESTS)
    find_package(Catch2 REQUIRED)
    find_package(benchmark REQUIRED)

    # Function to generate a test target
    function(make_unit_test test_name)
        add_executable(${test_name}
            ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cpp
            catch_app.cpp
        )

        target_include_directories(${test_name}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${KB_SOURCE_DIR}/source/kibble
        )

        set_target_properties(${test_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test
            OUTPUT_NAME ${test_name}
        )

        target_link_libraries(${test_name}
            PRIVATE
            project_options
            project_warnings
            Catch2::Catch2
            kibble
        )

        add_test(
            NAME ${test_name}
            COMMAND $<TARGET_FILE:${test_name}>
        )
    endfunction()

    # Global "tests" target to build all unit tests
    add_custom_target(kibble_tests)

    file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

    foreach(FILE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${FILE} NAME_WE)

        if(TEST_NAME MATCHES "^test_*")
            make_unit_test(${TEST_NAME})
            add_dependencies(kibble_tests "${TEST_NAME}")
            message(STATUS "Added unit test: ${TEST_NAME}")
        endif()
    endforeach()

    message(STATUS "All unit tests can be built at once with target 'kibble_tests'")

    # -------- BENCHMARKING -------- #
    function(make_benchmark bench_name)
        add_executable(${bench_name}
            ${CMAKE_CURRENT_SOURCE_DIR}/${bench_name}.cpp
        )

        target_include_directories(${bench_name}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${KB_SOURCE_DIR}/source/kibble
        )

        set_target_properties(${bench_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test
            OUTPUT_NAME ${bench_name}
            EXCLUDE_FROM_ALL 1
        )

        target_link_libraries(${bench_name}
            PRIVATE
            project_options
            kibble
            benchmark::benchmark
        )
    endfunction()

    # Global "bench" target to build all benchmarks
    add_custom_target(kibble_bench)

    file(GLOB BENCH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

    foreach(FILE ${BENCH_SOURCES})
        get_filename_component(BENCH_NAME ${FILE} NAME_WE)

        if(BENCH_NAME MATCHES "^bench_*")
            make_benchmark(${BENCH_NAME})
            add_dependencies(kibble_bench "${BENCH_NAME}")
            message(STATUS "Added benchmark: ${BENCH_NAME}")
        endif()
    endforeach()

    message(STATUS "All benchmarks can be built at once with target 'kibble_bench'")

    # -------- FUZZ TESTING -------- #
    add_executable(fuzz_tester fuzz_tester.cpp)

    target_include_directories(fuzz_tester
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${KB_SOURCE_DIR}/source/kibble
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

endif()