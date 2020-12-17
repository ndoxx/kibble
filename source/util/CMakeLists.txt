# -------- KPAK UTILITY -------- #

add_executable(kpak "${CMAKE_CURRENT_SOURCE_DIR}/kpak.cpp")

target_include_directories(kpak
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/source/kibble"
    )

target_compile_definitions(kpak PRIVATE -DLOGGING_ANSI_3=0)
target_compile_definitions(kpak PRIVATE -DLOGGING_ENABLED=1)

set_target_properties(kpak
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    )

target_link_libraries(kpak
    PRIVATE
        project_options
        project_warnings
        pthread
        stdc++fs
        kibble
    )

cotire(kpak)
install(TARGETS kpak DESTINATION bin)
