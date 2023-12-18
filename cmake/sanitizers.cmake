function(enable_sanitizers project_name)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        option(SAN_ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" FALSE)

        if(SAN_ENABLE_COVERAGE)
            target_compile_options(project_options INTERFACE --coverage -O0 -g)
            target_link_libraries(project_options INTERFACE --coverage)
        endif()

        set(SANITIZERS "")
        set(SANITIZER_OPTIONS "")

        option(SAN_ADDRESS "Enable address sanitizer" FALSE)

        if(SAN_ADDRESS)
            list(APPEND SANITIZERS "address")
        endif()

        option(SAN_MEMORY "Enable memory sanitizer" FALSE)
        option(SAN_MEMORY_TRACK_ORIGINS "Enable origins tracking (slower)" TRUE)

        if(SAN_MEMORY)
            list(APPEND SANITIZERS "memory")

            if(SAN_MEMORY_TRACK_ORIGINS)
                list(APPEND SANITIZER_OPTIONS -fno-omit-frame-pointer)
                list(APPEND SANITIZER_OPTIONS -fsanitize-memory-track-origins=2)
            endif()
        endif()

        option(SAN_UNDEFINED_BEHAVIOR
            "Enable undefined behavior sanitizer" FALSE)

        if(SAN_UNDEFINED_BEHAVIOR)
            list(APPEND SANITIZERS "undefined")
        endif()

        option(SAN_THREAD "Enable thread sanitizer" FALSE)

        if(SAN_THREAD)
            list(APPEND SANITIZERS "thread")
        endif()

        list(JOIN SANITIZERS "," LIST_OF_SANITIZERS)
    endif()

    if(LIST_OF_SANITIZERS)
        if(NOT "${LIST_OF_SANITIZERS}" STREQUAL "")
            target_compile_options(${project_name}
                INTERFACE -fsanitize=${LIST_OF_SANITIZERS} ${SANITIZER_OPTIONS} -fsanitize-ignorelist=${KB_SOURCE_DIR}/ignorelist.txt)
            target_link_libraries(${project_name}
                INTERFACE -fsanitize=${LIST_OF_SANITIZERS} ${SANITIZER_OPTIONS} -fsanitize-ignorelist=${KB_SOURCE_DIR}/ignorelist.txt)
        endif()
    endif()
endfunction()
