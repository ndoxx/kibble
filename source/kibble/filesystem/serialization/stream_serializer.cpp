#include "kibble/filesystem/serialization/stream_serializer.h"

namespace kb
{

StreamSerializer::StreamSerializer(std::ostream& stream) : stream_(stream)
{
}

StreamDeserializer::StreamDeserializer(std::istream& stream) : stream_(stream)
{
}

} // namespace erwin