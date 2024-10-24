cmake_minimum_required(VERSION 3.19)

project(kibble VERSION 1.2.4 DESCRIPTION "lib kibble" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(KIBBLE_VERSION ${PROJECT_VERSION})

# Check if we're building as a subproject
if(NOT DEFINED KB_MASTER_PROJECT)
	set(KB_MASTER_PROJECT OFF)

	if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
		set(KB_MASTER_PROJECT ON)
		message(STATUS "CMake version: ${CMAKE_VERSION}")
		message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
	endif()
endif()

set(KB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>)

if(KB_MASTER_PROJECT)
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		option(USE_LIBCXX "Use libc++ instead of libstdc++" OFF)

		if(USE_LIBCXX)
			message(STATUS "Using libc++")

			# Clang will complain if we try to compile C sources with this option
			add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
			add_link_options(-stdlib=libc++)

			# Check if we need to explicitly link against libc++abi
			include(CheckCXXSourceCompiles)
			check_cxx_source_compiles("
				#include <cstdlib>
				int main() { return 0; }
			" LIBCXX_WORKS)

			if(NOT LIBCXX_WORKS)
				link_libraries(c++abi)
			endif()
		endif()
	endif()
endif()

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
		${CMAKE_MODULE_PATH}/uninstall.cmake
		${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake
		IMMEDIATE @ONLY)

	add_custom_target(uninstall
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)
endif()

# Interface library to enable us to pass on options to multiple projects
if(NOT TARGET project_warnings)
	add_library(project_warnings INTERFACE)
endif()

set_project_warnings(project_warnings)

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
	setup_documentation(${DOCS_INPUT} ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
endif()

subdirs(
	source/kibble
	source/tests
	source/examples
	source/util
)
