# -------- KPAK UTILITY -------- #

add_executable(kpak "${CMAKE_CURRENT_SOURCE_DIR}/kpak.cpp")

target_include_directories(kpak
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${KB_SOURCE_DIR}/source/kibble"
    )

target_link_libraries(kpak
    PRIVATE
        project_options
        project_warnings
        stdc++fs
        kibble
    )

install(TARGETS kpak DESTINATION bin)


# -------- INTERN STRING SOURCE EXTRACTION UTILITY -------- #

add_executable(internstring "${CMAKE_CURRENT_SOURCE_DIR}/internstring.cpp")

target_include_directories(internstring
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${KB_SOURCE_DIR}/source/kibble"
    )

target_link_libraries(internstring
    PRIVATE
        project_options
        project_warnings
        stdc++fs
        kibble
    )

install(TARGETS internstring DESTINATION bin)