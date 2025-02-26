#include "kibble/filesystem/serialization/std_archiver.h"

namespace kb
{

bool Archiver<std::string>::write(const std::string& object, StreamSerializer& ser)
{
    // Write size first, then data
    uint64_t size = object.size();

    // First write the size
    if (!ser.write(size))
    {
        return false;
    }

    // Then write the raw bytes of the string (if size > 0)
    if (size > 0)
    {
        return ser.write_blob(object.data(), size);
    }

    return true;
}

bool Archiver<std::string>::read(std::string& object, StreamDeserializer& des)
{
    // Read size, then data
    uint64_t size;
    if (!des.read(size))
    {
        return false;
    }

    // Handle empty strings properly
    if (size == 0)
    {
        object.clear();
        return true;
    }

    // Resize the string to the expected size
    object.resize(size);

    // Read the raw bytes into the string
    return des.read_blob(object.data(), size);
}

bool Archiver<std::filesystem::path>::write(const std::filesystem::path& object, StreamSerializer& ser)
{
    return ser.write(object.generic_string());
}

bool Archiver<std::filesystem::path>::read(std::filesystem::path& object, StreamDeserializer& des)
{
    std::string path_str;
    if (des.read(path_str))
    {
        object = std::filesystem::path(path_str);
        return true;
    }
    return false;
}

} // namespace kb