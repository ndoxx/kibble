#include "filesystem/stream/packfile_stream.h"

namespace kb
{

PackFileStream::PackFileStreamBuf::PackFileStreamBuf(std::istream& base, std::streampos start, std::streamsize size,
                                                     std::streamsize buffer_size)
    : base_stream_(base), start_(start), size_(size), buffer_(size_t(buffer_size))
{
    base_stream_.seekg(start);
    setg(nullptr, nullptr, nullptr);
}

PackFileStream::PackFileStreamBuf::int_type PackFileStream::PackFileStreamBuf::underflow()
{
    if (gptr() == egptr())
    {
        std::streamsize bytes_to_read =
            std::min(static_cast<std::streamsize>(buffer_.size()), size_ - (base_stream_.tellg() - start_));
        if (bytes_to_read <= 0)
        {
            return traits_type::eof();
        }
        base_stream_.read(buffer_.data(), bytes_to_read);
        std::streamsize bytes_read = base_stream_.gcount();
        setg(buffer_.data(), buffer_.data(), buffer_.data() + bytes_read);
    }
    return gptr() == egptr() ? traits_type::eof() : traits_type::to_int_type(*gptr());
}

PackFileStream::PackFileStreamBuf::pos_type PackFileStream::PackFileStreamBuf::seekoff(off_type off,
                                                                                       std::ios_base::seekdir dir,
                                                                                       std::ios_base::openmode which)
{
    if (which & std::ios_base::in)
    {
        std::streampos new_pos;
        switch (dir)
        {
        case std::ios_base::beg:
            new_pos = start_ + off;
            break;
        case std::ios_base::cur:
            new_pos = base_stream_.tellg() - (egptr() - gptr()) + off;
            break;
        case std::ios_base::end:
            new_pos = start_ + size_ + off;
            break;
        default:
            return pos_type(off_type(-1));
        }
        if (new_pos < start_ || new_pos > start_ + size_)
        {
            return pos_type(off_type(-1));
        }
        base_stream_.seekg(new_pos);
        setg(nullptr, nullptr, nullptr);
        return new_pos - start_;
    }
    return pos_type(off_type(-1));
}

PackFileStream::PackFileStreamBuf::pos_type PackFileStream::PackFileStreamBuf::seekpos(pos_type pos,
                                                                                       std::ios_base::openmode which)
{
    return seekoff(pos, std::ios_base::beg, which);
}

PackFileStream::PackFileStream(std::istream& baseStream, std::streampos start, std::streamsize size)
    : std::istream(&buffer_), buffer_(baseStream, start, size)
{
}

} // namespace kb