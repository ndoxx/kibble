cmake_minimum_required(VERSION 3.10)

project(kibble VERSION 1.0.0 DESCRIPTION "lib kibble")

# if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
# set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
# endif()

# set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
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
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

add_library(project_options INTERFACE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
target_compile_features(project_options INTERFACE cxx_std_20)
enable_sanitizers(project_options)

set(DOCS_INPUT
	"${CMAKE_SOURCE_DIR}/source/kibble"
	"${CMAKE_SOURCE_DIR}/docs"
)

# -- Configuration --
option(KB_AREA_MEMORY_INITIALIZATION "Initialize heap area memory on creation" ON)
option(KB_JOB_SYSTEM_PROFILING "Gather worker idle/active time" ON)
option(KB_MORTON_INTRIN "Use the BMI2 instruction set for Morton encoding" ON)
option(KB_TARGET_DOCUMENTATION OFF)
option(KB_TARGET_INSTALL ON)
option(KB_TARGET_EXAMPLES ON)
option(KB_TARGET_TESTS ON)

if(KB_TARGET_DOCUMENTATION)
	setup_documentation("${DOCS_INPUT}" "${CMAKE_SOURCE_DIR}/doxygen")
endif()

subdirs(source)
