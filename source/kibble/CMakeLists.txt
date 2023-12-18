# -- Targets --
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
    PUBLIC
    "${KB_SOURCE_DIR}/source/vendor/fmt/include"
)

target_include_directories(kibble
    SYSTEM PRIVATE
    "${KB_SOURCE_DIR}/source/vendor"
    "${KB_SOURCE_DIR}/source/vendor/atomic_queue/include"
    "${KB_SOURCE_DIR}/source/vendor/tomlplusplus/include"
)

set_target_properties(kibble
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/random/uuid.cpp"
    PROPERTIES COMPILE_FLAGS -mavx2
)

if(KB_MORTON_INTRIN)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/math/morton.cpp"
        PROPERTIES COMPILE_FLAGS -mbmi2
    )
endif()

if(KB_JOB_SYSTEM_PROFILING)
    target_compile_definitions(kibble PUBLIC K_PROFILE_JOB_SYSTEM)
endif()

if(KB_AREA_MEMORY_INITIALIZATION)
    target_compile_definitions(kibble PUBLIC K_ENABLE_AREA_MEMORY_INITIALIZATION)
endif()

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    target_compile_definitions(kibble PUBLIC K_DEBUG)
endif()

# --------------------- [DEPENDENCIES] ---------------------

# ~~~~~~ fmtlib ~~~~~~
set(FMT_CUDA_TEST OFF)
set(FMT_DOC OFF)
set(FMT_FUZZ OFF)
set(FMT_INSTALL ON)
set(FMT_MODULE OFF)
set(FMT_OS ON)
set(FMT_PEDANTIC OFF)
set(FMT_SYSTEM_HEADERS OFF)
set(FMT_TEST OFF)
set(FMT_WERROR OFF)

add_subdirectory("${KB_SOURCE_DIR}/source/vendor/fmt" "external/fmt")
set_target_properties(fmt
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# ------------------------------------------
target_link_libraries(kibble
    PUBLIC
    fmt
    PRIVATE
    project_options
    project_warnings
    pthread
    atomic
    stdc++fs
)

# -- Install rules --
if(KB_TARGET_INSTALL)
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
endif()