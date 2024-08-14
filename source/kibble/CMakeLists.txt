# -- Targets --
file(GLOB_RECURSE SRC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(kibble STATIC)

target_sources(kibble
    PRIVATE
    ${SRC_LIB}
)

# CMake has no integer type (seriously guys...), we use string, she'll be fine.
set(KB_JOB_MAX_THREADS 8 CACHE STRING "Max number of worker threads")
set(KB_JOB_JOB_QUEUE_SIZE 1024 CACHE STRING "Max jobs per worker queue")
set(KB_JOB_STATS_QUEUE_SIZE 128 CACHE STRING "Max stats packets in monitor queue")
set(KB_JOB_MAX_CHILD 8 CACHE STRING "Max dependent jobs per job")
set(KB_JOB_MAX_PARENT 4 CACHE STRING "Max job dependencies")
option(KB_JOB_ATOMIC_POOL "Enable lock-free job pool in job system" ON)

configure_file(config.h.in config.h)

target_include_directories(kibble
    PUBLIC
    $<BUILD_INTERFACE:${KB_SOURCE_DIR}/source>
    $<INSTALL_INTERFACE:include>
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(kibble
    SYSTEM PRIVATE
    ${KB_SOURCE_DIR}/source/vendor
)

target_include_directories(kibble
    SYSTEM PUBLIC
    $<BUILD_INTERFACE:${KB_SOURCE_DIR}/source/vendor/atomic_queue/include>
    $<BUILD_INTERFACE:${KB_SOURCE_DIR}/source/vendor/fmt/include>
    $<INSTALL_INTERFACE:include>
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
    target_compile_definitions(kibble PUBLIC KB_JOB_SYSTEM_PROFILING)
endif()

option(KB_MEM_AREA_MEMSET "Initialize heap area memory on creation (DBG)" ON)
option(KB_MEM_MARK_PADDING "Initialize padded memory with a specific pattern (DBG)" ON)

if($<CONFIG:Debug>)
    if(KB_MEM_AREA_MEMSET)
        message(STATUS "MEM: Heap areas will be memset")
        target_compile_definitions(kibble PUBLIC K_USE_MEM_AREA_MEMSET)
    endif()

    if(KB_MEM_MARK_PADDING)
        message(STATUS "MEM: Padded memory will be marked")
        target_compile_definitions(kibble PUBLIC K_USE_MEM_MARK_PADDING)
    endif()
endif()

target_compile_definitions(kibble PUBLIC
    $<$<CONFIG:Debug>:K_DEBUG>
    $<$<CONFIG:Debug>:K_ENABLE_ASSERT>
)

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
    fmt::fmt
    PRIVATE
    project_options
    project_warnings
    pthread
    atomic
    stdc++fs
)

# -- Install rules --
if(KB_TARGET_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install library
    install(TARGETS kibble
        EXPORT kibbleTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(TARGETS project_options project_warnings EXPORT kibbleTargets)

    # Install headers
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
        PATTERN "impl" EXCLUDE
    )

    # Export targets
    install(EXPORT kibbleTargets
        FILE kibbleTargets.cmake
        NAMESPACE kibble::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kibble
    )

    # Create and install package config files
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/kibble-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/kibble-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kibble
        PATH_VARS CMAKE_INSTALL_INCLUDEDIR
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/kibble-config-version.cmake"
        VERSION ${KIBBLE_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/kibble-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/kibble-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kibble
    )
endif()