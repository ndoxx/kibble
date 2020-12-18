# -------- KPAK UTILITY -------- #

add_executable(kpak "${CMAKE_CURRENT_SOURCE_DIR}/kpak.cpp")

target_include_directories(kpak
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

target_compile_definitions(kpak PRIVATE -DLOGGING_ENABLED=1)

set_target_properties(kpak
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    )

target_link_libraries(kpak
    PRIVATE
        project_options
        project_warnings
        stdc++fs
        kibble
    )

cotire(kpak)
install(TARGETS kpak DESTINATION bin)


# -------- INTERN STRING SOURCE EXTRACTION UTILITY -------- #

add_executable(internstring "${CMAKE_CURRENT_SOURCE_DIR}/internstring.cpp")

target_include_directories(internstring
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

target_compile_definitions(internstring PRIVATE -DLOGGING_ENABLED=1)

set_target_properties(internstring
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    )

target_link_libraries(internstring
    PRIVATE
        project_options
        project_warnings
        stdc++fs
        kibble
    )

cotire(internstring)
install(TARGETS internstring DESTINATION bin)