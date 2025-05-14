#include "kibble/filesystem/serialization/stream_serializer.h"

namespace kb
{

constexpr uint32_t k_ser_version_tag = 0x53524556;

StreamSerializer::StreamSerializer(std::ostream& stream) : stream_(stream)
{
}

bool StreamSerializer::write_version(uint32_t version)
{
    // Write tag first
    if (!write(k_ser_version_tag))
    {
        return false;
    }
    // Write version info
    if (!write(version))
    {
        return false;
    }
    return true;
}

StreamDeserializer::StreamDeserializer(std::istream& stream) : stream_(stream)
{
}

bool StreamDeserializer::read_version(uint32_t& version)
{
    // Save current position
    auto current_pos = stream_.tellg();
    uint32_t ver_tag;
    if (read(ver_tag) && ver_tag == k_ser_version_tag)
    {
        // Magic number found, read version
        return read(version);
    }
    else
    {
        // Reset position to where we started
        stream_.clear();
        stream_.seekg(current_pos);
    }
    // Magic number not found, assume legacy format
    version = 0;
    return true;
}

} // namespace kb