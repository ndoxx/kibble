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
    ${KB_SOURCE_DIR}/source/vendor/cpptrace/include
)

target_include_directories(kibble
    SYSTEM PUBLIC
    $<BUILD_INTERFACE:${KB_SOURCE_DIR}/source/vendor/atomic_queue/include>
    $<BUILD_INTERFACE:${KB_SOURCE_DIR}/source/vendor/fmt/include>
)

set(KB_AVX2_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/random/impl/uuid_avx2.cpp
)
set(KB_BMI2_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/math/impl/morton_bmi2.cpp
)

set_source_files_properties(${KB_AVX2_FILES}
    PROPERTIES COMPILE_OPTIONS
        "$<IF:$<CXX_COMPILER_ID:MSVC>,/arch:AVX2,-mavx2>"
)
set_source_files_properties(${KB_BMI2_FILES}
    PROPERTIES COMPILE_OPTIONS
        "$<IF:$<CXX_COMPILER_ID:MSVC>,/arch:AVX2,-mbmi2>"
)

# Baseline: SSE4.2, SSSE3, POPCNT
target_compile_options(kibble PRIVATE
    -msse4.2 -mssse3 -mpopcnt
)

option(KB_JOB_SYSTEM_PROFILING "Gather worker idle/active time" ON)

if(KB_JOB_SYSTEM_PROFILING)
    target_compile_definitions(kibble PUBLIC KB_JOB_SYSTEM_PROFILING)
endif()

option(KB_MEM_AREA_MEMSET "Initialize heap area memory on creation (DBG)" ON)
option(KB_MEM_MARK_PADDING "Initialize padded memory with a specific pattern (DBG)" ON)

target_compile_definitions(kibble PUBLIC
    KIBBLE_BUILD_TYPE="$<CONFIG>"
    $<$<BOOL:${KB_MEM_AREA_MEMSET}>:K_USE_MEM_AREA_MEMSET>
    $<$<BOOL:${KB_MEM_MARK_PADDING}>:K_USE_MEM_MARK_PADDING>
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

mark_as_advanced(
    FMT_CMAKE_DIR
    FMT_DEBUG_POSTFIX
    FMT_INC_DIR
    FMT_LIB_DIR
    FMT_PKGCONFIG_DIR
    FMT_UNICODE
)

# ~~~~~~ cpptrace ~~~~~~
set(CPPTRACE_BUILD_TESTING OFF)
set(CPPTRACE_BUILD_BENCHMARKING OFF)
set(CPPTRACE_BUILD_TOOLS OFF)

add_subdirectory("${KB_SOURCE_DIR}/source/vendor/cpptrace" "external/cpptrace")

mark_as_advanced(
    BUILD_DWARFEXAMPLE
    BUILD_DWARFGEN
    BUILD_NON_SHARED
    BUILD_SHARED

    CPPTRACE_DEMANGLE_WITH_CXXABI
    CPPTRACE_DEMANGLE_WITH_NOTHING
    CPPTRACE_DEMANGLE_WITH_WINAPI
    CPPTRACE_FIND_LIBDWARF_WITH_PKGCONFIG
    CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE
    CPPTRACE_GET_SYMBOLS_WITH_DBGHELP
    CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE
    CPPTRACE_GET_SYMBOLS_WITH_LIBDL
    CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF
    CPPTRACE_GET_SYMBOLS_WITH_NOTHING
    CPPTRACE_HARD_MAX_FRAMES
    CPPTRACE_POSITION_INDEPENDENT_CODE
    CPPTRACE_STD_FORMAT
    CPPTRACE_UNPREFIXED_TRY_CATCH
    CPPTRACE_UNWIND_WITH_DBGHELP
    CPPTRACE_UNWIND_WITH_EXECINFO
    CPPTRACE_UNWIND_WITH_LIBUNWIND
    CPPTRACE_UNWIND_WITH_NOTHING
    CPPTRACE_UNWIND_WITH_UNWIND
    CPPTRACE_UNWIND_WITH_WINAPI
    CPPTRACE_USE_EXTERNAL_LIBDWARF
    CPPTRACE_USE_EXTERNAL_ZSTD
    CPPTRACE_ZSTD_URL

    DO_TESTING
    ENABLE_DECOMPRESSION

    FETCHCONTENT_BASE_DIR
    FETCHCONTENT_FULLY_DISCONNECTED
    FETCHCONTENT_QUIET
    FETCHCONTENT_SOURCE_DIR_LIBDWARF
    FETCHCONTENT_SOURCE_DIR_ZSTD
    FETCHCONTENT_UPDATES_DISCONNECTED
    FETCHCONTENT_UPDATES_DISCONNECTED_LIBDWARF
    FETCHCONTENT_UPDATES_DISCONNECTED_ZSTD

    LIBDWARF_CRT
    LIBDWARF_STATIC

    WALL

    ZDICTLIB_STATIC_API
    ZDICTLIB_VISIBLE
    ZSTDERRORLIB_VISIBLE
    ZSTDLIB_STATIC_API
    ZSTDLIB_VISIBLE
    ZSTD_BUILD_COMPRESSION
    ZSTD_BUILD_DECOMPRESSION
    ZSTD_BUILD_DEPRECATED
    ZSTD_BUILD_DICTBUILDER
    ZSTD_MULTITHREAD_SUPPORT
    ZSTD_PROGRAMS_LINK_SHARED
)

# ------------------------------------------
target_link_libraries(kibble
    PUBLIC
    fmt::fmt
    PRIVATE
    project_options
    project_warnings
    cpptrace::cpptrace
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(kibble
        PUBLIC
        pthread
        atomic
        stdc++fs
    )
endif()
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(kibble
        PRIVATE
        ws2_32
    )
endif()

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