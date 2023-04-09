function(make_example_app project_name)
    set(NAME "ex_${project_name}")

    add_executable(${NAME} "${CMAKE_CURRENT_SOURCE_DIR}/${project_name}.cpp")
    target_include_directories(${NAME}
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

    set_target_properties(${NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/ex"
        OUTPUT_NAME "${project_name}"
        EXCLUDE_FROM_ALL 1
    )

    target_link_libraries(${NAME}
        PRIVATE
        project_options
        project_warnings
        pthread
        stdc++fs
        kibble
    )
endfunction()

add_custom_target(examples)

# Loop over all cpp files in the example directory
file(GLOB EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(FILE ${EXAMPLE_SOURCES})
    # Extract file name without extension and use this to declare a new target
    get_filename_component(EX_NAME ${FILE} NAME_WE)
    make_example_app(${EX_NAME})

    # Add dependency to the group target examples,
    # then we can do "make examples" to build all of them
    add_dependencies(examples "ex_${EX_NAME}")
    message(STATUS "Added example: ${EX_NAME}")
endforeach()

message(STATUS "All examples can be built at once with 'make examples'")
