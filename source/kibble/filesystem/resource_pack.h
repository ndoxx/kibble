#pragma once

#include <filesystem>

namespace fs = std::filesystem;
namespace kb
{
namespace kfs
{

bool pack_directory(const fs::path& dir_path, const fs::path& archive_path);


} // namespace kfs
} // namespace kb