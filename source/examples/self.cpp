#include "self/self.h"

#include "fmt/core.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /*
        This is how you can retrieve project version information programmatically.

        The CMake script generates a "config.h" file in the binary (build) directory
        during build time. This file is instantiated from the "config.h.in" file located
        in the kibble source root, CMake populates all the #define directives with the
        values of its internal variables. The config.h file is accessible (privately)
        in the project, and in particular in self/self.h where the kibble_project_X()
        functions are defined.
    */

    fmt::println("project name:  {}", kb::kibble_project_name());
    fmt::println("build type:    {}", kb::kibble_project_build_type());
    fmt::println("version:       {}", kb::kibble_project_version());
    fmt::println("version major: {}", kb::kibble_project_version_major());
    fmt::println("version minor: {}", kb::kibble_project_version_minor());
    fmt::println("version patch: {}", kb::kibble_project_version_patch());

    return 0;
}