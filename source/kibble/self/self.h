#pragma once

#include <cstdint>
#include <string_view>

namespace kb
{

/**
 * @brief Get the name of the Kibble project, should be "kibble"
 *
 * @return std::string_view
 */
std::string_view kibble_project_name();

/**
 * @brief Get a string describing the build type ("Debug", "Release"...)
 *
 * @return std::string_view
 */
std::string_view kibble_project_build_type();

/**
 * @brief Get the dot separated version string of the Kibble project
 *
 * @return std::string_view
 */
std::string_view kibble_project_version();

/**
 * @brief Get the integer project version major
 *
 * @return uint8_t
 */
uint8_t kibble_project_version_major();

/**
 * @brief Get the integer project version minor
 *
 * @return uint8_t
 */
uint8_t kibble_project_version_minor();

/**
 * @brief Get the integer project version patch
 *
 * @return uint8_t
 */
uint8_t kibble_project_version_patch();

} // namespace kb