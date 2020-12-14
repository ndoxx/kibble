#include "filesystem/resource_pack.h"
#include "logger/logger.h"
#include "string/string.h"
#include "assert/assert.h"

#include <fstream>
#include <vector>

namespace kb
{
namespace kfs
{

struct PAKHeader
{
    uint32_t magic;         // Magic number to check file format validity
    uint16_t version_major; // Version major number
    uint16_t version_minor; // Version minor number
    uint32_t entry_count;   // Number of file entries in this pack
};

struct IndexData
{
	uint32_t offset;
	uint32_t size;
	uint32_t path_len;
};

#define KPAK_MAGIC 0x4b41504b // ASCII(KPAK)
#define KPAK_VERSION_MAJOR 0
#define KPAK_VERSION_MINOR 1

bool pack_directory(const fs::path& dir_path, const fs::path& archive_path)
{
	if(!fs::exists(dir_path))
	{
		KLOGE("ios") << "[kpak] Directory does not exist:" << std::endl;
		KLOGI << WCC('p') << dir_path << std::endl;
		return false;
	}

	PAKHeader h;
	h.magic = KPAK_MAGIC;
	h.version_major = KPAK_VERSION_MAJOR;
	h.version_minor = KPAK_VERSION_MINOR;

	struct FileRequest
	{
		std::uintmax_t size;
		fs::path filepath;
	};

	std::vector<FileRequest> requests;
	size_t current_offset = sizeof(PAKHeader);
	std::uintmax_t max_file_size = 0;

	// Explore
	for(auto& entry: fs::recursive_directory_iterator(dir_path))
	{
		if(entry.is_regular_file())
		{
			fs::path rel_path = fs::relative(entry.path(), dir_path);
			current_offset += sizeof(IndexData) + rel_path.string().size();
			requests.push_back({entry.file_size(), rel_path});
			if(entry.file_size() > max_file_size)
				max_file_size = entry.file_size();
		}
	}

	h.entry_count = uint32_t(requests.size());

	KLOG("ios",0) << "[kpak] Packing directory:" << std::endl;
	KLOGI << WCC('p') << dir_path << std::endl;
	KLOG("ios",0) << "[kpak] Target archive:" << std::endl;
	KLOGI << WCC('p') << archive_path << std::endl;

	// Write header
	std::ofstream ofs(archive_path, std::ios::binary);
	ofs.write(reinterpret_cast<const char*>(&h), sizeof(PAKHeader));

	// Write index
	for(const auto& req: requests)
	{
		std::string path_str(req.filepath.string());
		IndexData idata;
		idata.offset = uint32_t(current_offset);
		idata.size = uint32_t(req.size);
		idata.path_len = uint32_t(path_str.size());

		ofs.write(reinterpret_cast<const char*>(&idata), sizeof(IndexData));
		ofs.write(reinterpret_cast<const char*>(path_str.data()), long(path_str.size()));

		current_offset += size_t(req.size);
	}

	// Write all files to pack
	std::vector<char> databuf;
	databuf.reserve(size_t(max_file_size));
	for(const auto& req: requests)
	{
		KLOG("ios",0) << "[kpak] " << req.filepath << " (" << su::size_to_string(req.size) << ')' << std::endl;
		std::ifstream ifs(dir_path / req.filepath, std::ios::binary);
		databuf.insert(databuf.begin(), std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
		ofs.write(databuf.data(), long(databuf.size()));
		databuf.clear();
	}

	return true;
}

PackFile::PackFile(const fs::path& filepath):
filepath_(filepath)
{
	K_ASSERT(fs::exists(filepath), "Pack file does not exist.");
	std::ifstream ifs(filepath, std::ios::binary);
	
	// Read header & sanity check
	PAKHeader h;
	ifs.read(reinterpret_cast<char*>(&h), sizeof(PAKHeader));
    K_ASSERT(h.magic == KPAK_MAGIC, "Invalid KPAK file: magic number mismatch.");
    K_ASSERT(h.version_major == KPAK_VERSION_MAJOR, "Invalid KPAK file: version (major) mismatch.");
    K_ASSERT(h.version_minor == KPAK_VERSION_MINOR, "Invalid KPAK file: version (minor) mismatch.");

    // Read index
    for(size_t ii=0; ii<h.entry_count; ++ii)
    {
		IndexData idata;
		ifs.read(reinterpret_cast<char*>(&idata), sizeof(IndexData));
    	IndexTableEntry entry;
    	entry.offset = idata.offset;
    	entry.size = idata.size;
    	entry.path.resize(idata.path_len);
		ifs.read(entry.path.data(), idata.path_len);
    	hash_t hname = H_(entry.path);
    	index_.insert({hname, std::move(entry)});
    }

}

const IndexTableEntry& PackFile::get_entry(const std::string& path)
{
	hash_t hname = H_(path);
	auto findit = index_.find(hname);
	K_ASSERT(findit != index_.end(), "Unknown entry.");
	return findit->second;
}


} // namespace kfs
} // namespace kb