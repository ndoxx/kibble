# --------------------- [UNIT TESTS] ---------------------

find_package(Catch2 REQUIRED)

# Function to generate a test target
function(make_unit_test test_name)
    add_executable(${test_name}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/catch_app.cpp
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

# Global "kibble_tests" target to build all unit tests
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
