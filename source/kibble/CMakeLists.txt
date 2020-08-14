file(GLOB_RECURSE SRC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(kibble STATIC)

target_sources(kibble
    PRIVATE
        ${SRC_LIB}
    )

target_include_directories(kibble
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

set_target_properties(kibble
    PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
        POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(kibble
    PRIVATE
        project_options
        project_warnings
        pthread
        stdc++fs
)

cotire(kibble)

# Install rules
install(TARGETS kibble DESTINATION lib)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/hash/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/hash)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/logger/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/logger)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/time/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/time)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/net/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/net)

## More elegant, but DNW as dir is absolute
# file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
# foreach ( file ${H_LIB} )
#     get_filename_component(dir ${file} DIRECTORY)
#     install(FILES ${file} DESTINATION include/kibble/${dir})
# endforeach()
