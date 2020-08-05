file(GLOB_RECURSE SRC_APP "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(test_kibble)
target_sources(test_kibble
    PRIVATE
        ${SRC_APP}
    )
target_include_directories(test_kibble
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

target_compile_definitions(test_kibble PRIVATE -DLOGGING_ANSI_3=0)
target_compile_definitions(test_kibble PRIVATE -DLOGGING_ENABLED=1)

set_target_properties(test_kibble
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_link_libraries(test_kibble
    PRIVATE
        project_options
        project_warnings
        pthread
        stdc++fs
        kibble
)

cotire(test_kibble)