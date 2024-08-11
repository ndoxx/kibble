cmake_minimum_required(VERSION 3.19)

project(kibble VERSION 1.2.3 DESCRIPTION "lib kibble" LANGUAGES CXX)
set(KIBBLE_VERSION ${PROJECT_VERSION})

# Check if we're building as a subproject
if(NOT DEFINED KB_MASTER_PROJECT)
	set(KB_MASTER_PROJECT OFF)

	if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
		set(KB_MASTER_PROJECT ON)
		message(STATUS "CMake version: ${CMAKE_VERSION}")
		message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
		message(STATUS "Build: [${CMAKE_BUILD_TYPE}]")
	endif()
endif()

set(KB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(KB_MASTER_PROJECT)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${KB_SOURCE_DIR}/lib")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${KB_SOURCE_DIR}/lib")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${KB_SOURCE_DIR}/bin")
endif()

# if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
# set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
# endif()

# set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MODULE_PATH "${KB_SOURCE_DIR}/cmake/")
include(standard_settings)
include(compiler_warnings) # Hardcore warnings
include(sanitizers)
include(documentation)
include(clang-cxx-dev-tools)

# -- Uninstall target --
if(NOT TARGET uninstall)
	configure_file(
		"${CMAKE_MODULE_PATH}/uninstall.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
		IMMEDIATE @ONLY)

	add_custom_target(uninstall
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)
endif()

# Interface library to enable us to pass on options to multiple projects
if(NOT TARGET project_warnings)
	add_library(project_warnings INTERFACE)
endif()

set_project_warnings(project_warnings)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET project_options)
	add_library(project_options INTERFACE)
endif()

target_compile_features(project_options INTERFACE cxx_std_20)
enable_sanitizers(project_options)

set(DOCS_INPUT
	"${KB_SOURCE_DIR}/source/kibble"
	"${KB_SOURCE_DIR}/docs"
)

# -- Configuration --
option(KB_TARGET_DOCUMENTATION OFF)
option(KB_TARGET_INSTALL ${KB_MASTER_PROJECT})
option(KB_TARGET_EXAMPLES ${KB_MASTER_PROJECT})
option(KB_TARGET_TESTS ${KB_MASTER_PROJECT})

if(KB_TARGET_DOCUMENTATION)
	setup_documentation("${DOCS_INPUT}" "${KB_SOURCE_DIR}/doxygen")
endif()

subdirs(source)
