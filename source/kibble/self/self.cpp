#include "kibble/self/self.h"
#include "config.h"

namespace kb
{

static constexpr std::string_view k_project_name = KIBBLE_NAME;
static constexpr std::string_view k_project_build_type = KIBBLE_BUILD_TYPE;
static constexpr std::string_view k_project_version_string = KIBBLE_VERSION;

std::string_view kibble_project_name()
{
    return k_project_name;
}

std::string_view kibble_project_build_type()
{
    return k_project_build_type;
}

std::string_view kibble_project_version()
{
    return k_project_version_string;
}

uint8_t kibble_project_version_major()
{
    return KIBBLE_VERSION_MAJOR;
}

uint8_t kibble_project_version_minor()
{
    return KIBBLE_VERSION_MINOR;
}

uint8_t kibble_project_version_patch()
{
    return KIBBLE_VERSION_PATCH;
}

} // namespace kb