add_executable(test_logger)
target_sources(test_logger
    PRIVATE
        test_logger.cpp
    )
target_include_directories(test_logger
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

target_compile_definitions(test_logger PRIVATE -DLOGGING_ANSI_3=0)
target_compile_definitions(test_logger PRIVATE -DLOGGING_ENABLED=1)

set_target_properties(test_logger
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/test"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/test"
)

target_link_libraries(test_logger
    PRIVATE
        project_options
        project_warnings
        pthread
        stdc++fs
        kibble
)

cotire(test_logger)



set(SRC_KIBBLE_TEST
    catch_app.cpp
    test_ap.cpp
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

cotire(test_kibble)