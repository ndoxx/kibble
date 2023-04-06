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

target_include_directories(kibble
    SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/atomic_queue/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/tomlplusplus/include"
)

set_target_properties(kibble
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    POSITION_INDEPENDENT_CODE ON
)

set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/random/uuid.cpp"
    PROPERTIES COMPILE_FLAGS -mavx2
)

# Configuration
option(ENABLE_JOB_SYSTEM_PROFILING "Gather worker idle/active time" ON)
option(ENABLE_WORK_STEALING "Allow workers to steal jobs when their queue is empty" ON)

if(ENABLE_JOB_SYSTEM_PROFILING)
    target_compile_definitions(kibble PUBLIC K_PROFILE_JOB_SYSTEM)
endif()

if(ENABLE_WORK_STEALING)
    target_compile_definitions(kibble PUBLIC K_ENABLE_WORK_STEALING)
endif()

target_link_libraries(kibble
    PRIVATE
    project_options
    project_warnings
    pthread
    atomic
    stdc++fs
    fmt
)

# -- Install rules --
# Install library
install(TARGETS kibble DESTINATION lib)

# Install all API header files
file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

foreach(file ${H_LIB})
    get_filename_component(absolute_dir ${file} DIRECTORY)

    # Get relative path
    file(RELATIVE_PATH relative_dir ${CMAKE_CURRENT_SOURCE_DIR} ${absolute_dir})

    # Check if directory is called "impl"
    cmake_path(GET relative_dir STEM LAST_ONLY stem_dir)

    # Install if not an impl directory
    if(NOT stem_dir STREQUAL "impl")
        install(FILES ${file} DESTINATION include/kibble/${relative_dir})
    endif()
endforeach()
