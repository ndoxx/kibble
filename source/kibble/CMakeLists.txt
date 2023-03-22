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
option(ENABLE_ASSERTIONS "Enable Kibble assertions" ON)
option(ENABLE_LOGGING "Enable logging within Kibble" ON)
option(ENABLE_JOB_SYSTEM_PROFILING "Gather worker idle/active time" ON)
option(ENABLE_JOB_SYSTEM_EXCEPTIONS "Make job system handle exceptions properly" ON)
option(ENABLE_WORK_STEALING "Allow workers to steal jobs when their queue is empty" ON)
option(ENABLE_SHARED_STATE_PAGE_ALIGN "Page align job system shared state members" ON)

if(ENABLE_LOGGING)
    target_compile_definitions(kibble PRIVATE LOGGING_ENABLED=1)
else()
    target_compile_definitions(kibble PRIVATE LOGGING_ENABLED=0)
endif()

if(ENABLE_ASSERTIONS)
    target_compile_definitions(kibble PRIVATE K_ENABLE_ASSERT)
endif()

if(ENABLE_JOB_SYSTEM_PROFILING)
    target_compile_definitions(kibble PUBLIC K_PROFILE_JOB_SYSTEM)
endif()

if(ENABLE_JOB_SYSTEM_EXCEPTIONS)
    target_compile_definitions(kibble PUBLIC K_ENABLE_JOB_EXCEPTIONS)
endif()

if(ENABLE_WORK_STEALING)
    target_compile_definitions(kibble PUBLIC K_ENABLE_WORK_STEALING)
endif()

if(ENABLE_SHARED_STATE_PAGE_ALIGN)
    target_compile_definitions(kibble PUBLIC K_ENABLE_SHARED_STATE_PAGE_ALIGN)
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

# Install rules
install(TARGETS kibble DESTINATION lib)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/algorithm/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/algorithm)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/argparse/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/argparse)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/assert/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/assert)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/cli/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/cli)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/config/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/config)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/ctti/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/ctti)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/event/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/event)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/filesystem/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/filesystem)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/hash/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/hash)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/logger/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/logger)

file(GLOB H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/logger2/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/logger2)
file(GLOB H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/logger2/formatters/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/logger2/formatters)
file(GLOB H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/logger2/policies/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/logger2/policies)
file(GLOB H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/logger2/sinks/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/logger2/sinks)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/math/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/math)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/memory/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/memory)
file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/memory/*.inc")
install(FILES ${H_LIB} DESTINATION include/kibble/memory)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/net/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/net)

file(GLOB H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/random/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/random)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/string/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/string)

file(GLOB H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/thread/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/thread)
file(GLOB H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/thread/job/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/thread/job)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/time/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/time)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/undo/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/undo)

file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/util/*.h")
install(FILES ${H_LIB} DESTINATION include/kibble/util)

## More elegant, but DNW as dir is absolute
# file(GLOB_RECURSE H_LIB "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
# foreach ( file ${H_LIB} )
#     get_filename_component(dir ${file} DIRECTORY)
#     install(FILES ${file} DESTINATION include/kibble/${dir})
# endforeach()
