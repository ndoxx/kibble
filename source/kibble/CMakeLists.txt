# -- Targets --
file(GLOB_RECURSE SRC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(kibble STATIC)

target_sources(kibble
    PRIVATE
    ${SRC_LIB}
)

configure_file(config.h.in config.h)

target_include_directories(kibble
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
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

option(KB_MORTON_INTRIN "Use the BMI2 instruction set for Morton encoding" ON)

if(KB_MORTON_INTRIN)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/math/morton.cpp"
        PROPERTIES COMPILE_FLAGS -mbmi2
    )
endif()

option(KB_JOB_SYSTEM_PROFILING "Gather worker idle/active time" ON)

if(KB_JOB_SYSTEM_PROFILING)
    target_compile_definitions(kibble PUBLIC K_USE_JOB_SYSTEM_PROFILING)
endif()

option(KB_MEM_AREA_MEMSET "Initialize heap area memory on creation (DBG)" ON)
option(KB_MEM_MARK_PADDING "Initialize padded memory with a specific pattern (DBG)" ON)

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    if(KB_MEM_AREA_MEMSET)
        message(STATUS "MEM: Heap areas will be memset")
        target_compile_definitions(kibble PUBLIC K_USE_MEM_AREA_MEMSET)
    endif()

    if(KB_MEM_MARK_PADDING)
        message(STATUS "MEM: Padded memory will be marked")
        target_compile_definitions(kibble PUBLIC K_USE_MEM_MARK_PADDING)
    endif()

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
    install(
        TARGETS kibble
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )

    # Installation directories
    include(GNUInstallDirs)
    set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/kibble"
        CACHE PATH "Location of header files" )

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
            install(FILES ${file} DESTINATION "${INCLUDE_INSTALL_DIR}/${relative_dir}")
        endif()
    endforeach()

    # Generate and install package config files so kibble can be located with find_package()
    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/kibble-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/kibble-config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/kibble"
        PATH_VARS
        INCLUDE_INSTALL_DIR
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/kibble-config-version.cmake"
        VERSION ${KIBBLE_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    install(
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/kibble-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/kibble-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/kibble"
    )
endif()