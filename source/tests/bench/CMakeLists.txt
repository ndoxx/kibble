# --------------------- [BENCHMARKS] ---------------------

find_package(benchmark REQUIRED)

# Function to generate a bench target
function(make_benchmark test_name)
    add_executable(${test_name}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cpp
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
        EXCLUDE_FROM_ALL 1
    )

    target_link_libraries(${test_name}
        PRIVATE
        project_options
        kibble
        benchmark::benchmark
    )
endfunction()

# Global "kibble_bench" target to build all benchmarks
add_custom_target(kibble_bench)

file(GLOB BENCH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(FILE ${BENCH_SOURCES})
    get_filename_component(TEST_NAME ${FILE} NAME_WE)

    if(TEST_NAME MATCHES "^bench_*")
        make_benchmark(${TEST_NAME})
        add_dependencies(kibble_bench "${TEST_NAME}")
        message(STATUS "Added benchmark: ${TEST_NAME}")
    endif()
endforeach()

message(STATUS "All benchmarks can be built at once with target 'kibble_bench'")