#pragma once

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace kb::kfs
{

class SaveFile
{
public:
    enum class ErrorCode
    {
        NO_ERROR,
        WARN_ALREADY_COMMITTED,
        ERROR_BAD_STREAM,
        ERROR_CANT_RENAME
    };

    SaveFile(const fs::path& filepath, std::ios::openmode mode = std::ios::binary | std::ios::out);

    ~SaveFile();

    /**
     * @brief Call this once you're done with the stream.
     *
     * Make sure that the temporary file is valid (compute a checksum, etc.) before calling this.
     * The stream will be closed. Any stream error will cause the temporary file to be deleted, and
     * stop the save process.
     * If everything is fine, the temporary file will be moved to the destination, and the old destination
     * file will be renamed to the backup beforehand.
     *
     * @return SaveFile::ErrorCode Error code
     */
    ErrorCode commit() noexcept;

    inline std::ofstream& stream()
    {
        return tmp_stream_;
    }
    inline std::ofstream& operator*()
    {
        return stream();
    }
    inline const fs::path& get_temporary_path() const
    {
        return tmp_path_;
    }

private:
    std::ofstream tmp_stream_;
    fs::path tmp_path_;
    fs::path bak_path_;
    fs::path dst_path_;
    bool committed_{false};
};

} // namespace kb::kfs