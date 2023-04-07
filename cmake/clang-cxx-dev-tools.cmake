# Based on https://www.labri.fr/perso/fleury/posts/programming/using-clang-tidy-and-clang-format.html

# List all source files
file(GLOB_RECURSE
    ALL_CXX_SOURCE_FILES
    *.h *.hpp *.cpp
)

# Exclude third party stuff
list(FILTER ALL_CXX_SOURCE_FILES EXCLUDE REGEX "/source/vendor/")

# Adding clang-format target if executable is found
find_program(CLANG_FORMAT "clang-format")

if(CLANG_FORMAT)
    add_custom_target(
        clang-format
        COMMAND clang-format
        -i
        -style=file
        ${ALL_CXX_SOURCE_FILES}
    )
    message(STATUS "Added target 'clang-format'")
endif()
