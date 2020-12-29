
function(make_example_app project_name)

    set(NAME "ex_${project_name}")

    add_executable(${NAME} "${CMAKE_CURRENT_SOURCE_DIR}/${project_name}.cpp")

    target_include_directories(${NAME}
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_SOURCE_DIR}/source/kibble"
        )
    target_compile_definitions(${NAME} PRIVATE -DLOGGING_ENABLED=1)

    set_target_properties(${NAME}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/ex"
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

    cotire(${NAME})

endfunction()

make_example_app(argparse)
make_example_app(catenary)
make_example_app(config)
make_example_app(easing)
make_example_app(filesystem)
make_example_app(job)
make_example_app(spline)
make_example_app(simplex_noise)
