cmake_minimum_required(VERSION 3.10)

project(kibble VERSION 1.0.0 DESCRIPTION "lib kibble")

set(CMAKE_BUILD_TYPE Release)
# set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE MinSizeRel)
# set(CMAKE_BUILD_TYPE RelWithDebInfo)

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
	add_definitions(-DK_DEBUG)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
include(cotire)
include(standard_settings)
include(compiler_warnings) # Hardcore warnings
include(sanitizers)

# Interface library to enable us to pass on options to multiple projects
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)
enable_sanitizers(project_options)

subdirs(source)
