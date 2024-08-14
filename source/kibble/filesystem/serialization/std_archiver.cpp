#include "kibble/filesystem/serialization/std_archiver.h"

namespace kb
{

bool Archiver<std::string>::write(const std::string& object, StreamSerializer& ser)
{
    // Write size first, then data
    size_t size = object.size();
    return ser.write(size) && ser.write_blob(object.data(), size);
}

bool Archiver<std::string>::read(std::string& object, StreamDeserializer& des)
{
    // Read size, then data
    size_t size;
    if (!des.read(size))
    {
        return false;
    }
    object.resize(size);
    return des.read_blob(object.data(), size);
}

bool Archiver<std::filesystem::path>::write(const std::filesystem::path& object, StreamSerializer& ser)
{
    return ser.write(object.string());
}

bool Archiver<std::filesystem::path>::read(std::filesystem::path& object, StreamDeserializer& des)
{
    std::string path_str;
    if (des.read(path_str))
    {
        object = path_str;
        return true;
    }
    return false;
}

} // namespace kb