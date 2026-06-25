# --------------------- [FUZZ TESTING] ---------------------

# Function to generate a fuzz target
function(make_fuzz_test test_name)
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
    )
endfunction()

# Global "kibble_fuzz" target to build all fuzz tests
add_custom_target(kibble_fuzz)

file(GLOB FUZZ_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(FILE ${FUZZ_SOURCES})
    get_filename_component(TEST_NAME ${FILE} NAME_WE)

    if(TEST_NAME MATCHES "^fuzz_*")
        make_fuzz_test(${TEST_NAME})
        add_dependencies(kibble_fuzz "${TEST_NAME}")
        message(STATUS "Added fuzz test: ${TEST_NAME}")
    endif()
endforeach()

message(STATUS "All fuzz tests can be built at once with target 'kibble_fuzz'")