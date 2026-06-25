# add_executable(fuzz_tester fuzz_tester.cpp)

# target_include_directories(fuzz_tester
#     PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${KB_SOURCE_DIR}/source/kibble
# )

# set_target_properties(fuzz_tester
#     PROPERTIES
#     EXCLUDE_FROM_ALL 1
# )

# target_link_libraries(fuzz_tester
#     PRIVATE
#     project_options
#     project_warnings
#     kibble
#     -coverage
#     -fsanitize=fuzzer,undefined,address
# )

# target_compile_options(fuzz_tester
#     PRIVATE
#     -fsanitize=fuzzer,undefined,address
# )