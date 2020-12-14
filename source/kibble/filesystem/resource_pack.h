#pragma once

#include <filesystem>
#include <map>

#include "../hash/hashstr.h"

namespace fs = std::filesystem;
namespace kb
{
namespace kfs
{

bool pack_directory(const fs::path& dir_path, const fs::path& archive_path);

struct IndexTableEntry
{
	uint32_t offset;
	uint32_t size;
	std::string path;
};

class PackFile
{
public:
	PackFile(const fs::path& filepath);

	const IndexTableEntry& get_entry(const std::string& path);

private:
	fs::path filepath_;
	std::map<hash_t, IndexTableEntry> index_;
};

} // namespace kfs
} // namespace kb