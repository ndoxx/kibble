#include "filesystem/save_file.h"
#include "random/uuid.h"
#include "random/xor_shift.h"

namespace kb::kfs
{

SaveFile::SaveFile(const fs::path& filepath, std::ios::openmode mode)
{
    // Create UUID engine
    kb::UUIDv4::UUIDGenerator<kb::rng::XorShiftEngine> uuid;
    // Get parent directory
    auto parent = filepath.parent_path();
    // Create temporary file
    tmp_path_ = parent / (uuid().str() + ".tmp" + filepath.extension().string());
    tmp_stream_.open(tmp_path_.string(), mode);
    // Create backup
    bak_path_ = parent / (filepath.filename().string() + ".bak");
    dst_path_ = filepath;
}

SaveFile::~SaveFile()
{
    // Just remove the temporary file
    if (!committed_ && fs::exists(tmp_path_))
    {
        tmp_stream_.close();
        std::error_code ec;
        fs::remove(tmp_path_, ec);
    }
}

SaveFile::ErrorCode SaveFile::commit() noexcept
{
    if (committed_)
        return ErrorCode::WARN_ALREADY_COMMITTED;

    tmp_stream_.close();
    if (!tmp_stream_.good())
    {
        std::error_code ec;
        fs::remove(tmp_path_, ec);
        return ErrorCode::ERROR_BAD_STREAM;
    }

    // Move file to backup
    if (fs::exists(bak_path_))
        fs::remove(bak_path_);

    try
    {
        if (fs::exists(dst_path_))
            fs::rename(dst_path_, bak_path_);
        // Move temporary file to destination
        fs::rename(tmp_path_, dst_path_);
    }
    catch (...)
    {
        return ErrorCode::ERROR_CANT_RENAME;
    }

    // Remove temporary file
    std::error_code ec;
    fs::remove(tmp_path_, ec);

    committed_ = true;
    return ErrorCode::NO_ERROR;
}

} // namespace kb::kfs