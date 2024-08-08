#include "filesystem/stream/memory_stream.h"

namespace kb
{

MemoryBuffer::MemoryBuffer(void* buffer, size_t size) : begin_(reinterpret_cast<char*>(buffer)), end_(begin_ + size)
{
    if (buffer == nullptr)
    {
        throw std::runtime_error("MemoryBuffer: input buffer pointer is null");
    }
    if (size == 0)
    {
        throw std::runtime_error("MemoryBuffer: input buffer size is null");
    }

    setg(begin_, begin_, end_);
    setp(begin_, end_);
}

MemoryBuffer::pos_type MemoryBuffer::seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which)
{
    char* new_pos;
    if (dir == std::ios_base::beg)
    {
        new_pos = begin_ + off;
    }
    else if (dir == std::ios_base::cur)
    {
        new_pos = (which & std::ios_base::in) ? gptr() + off : pptr() + off;
    }
    else if (dir == std::ios_base::end)
    {
        new_pos = end_ + off;
    }
    else
    {
        return pos_type(off_type(-1));
    }

    if (new_pos < begin_ || new_pos >= end_)
    {
        return pos_type(off_type(-1));
    }
    if (which & std::ios_base::in)
    {
        setg(begin_, new_pos, end_);
    }
    if (which & std::ios_base::out)
    {
        setp(new_pos, end_);
    }
    return pos_type(new_pos - begin_);
}

MemoryBuffer::pos_type MemoryBuffer::seekpos(pos_type pos, std::ios_base::openmode which)
{
    return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, which);
}

std::streamsize MemoryBuffer::xsgetn(char* s, std::streamsize n)
{
    std::streamsize avail = std::min(n, std::streamsize(egptr() - gptr()));
    if (avail > 0)
    {
        std::memcpy(s, gptr(), size_t(avail));
        gbump(int(avail));
    }
    return avail;
}

std::streamsize MemoryBuffer::xsputn(const char* s, std::streamsize n)
{
    std::streamsize avail = std::min(n, std::streamsize(epptr() - pptr()));
    if (avail > 0)
    {
        std::memcpy(pptr(), s, size_t(avail));
        pbump(int(avail));
    }
    return avail;
}

MemoryBuffer::int_type MemoryBuffer::overflow(int_type ch)
{
    if (ch != traits_type::eof())
    {
        if (pptr() < epptr())
        {
            *pptr() = char_type(ch);
            pbump(1);
            return ch;
        }
    }
    return traits_type::eof();
}

MemoryBuffer::int_type MemoryBuffer::underflow()
{
    if (gptr() < egptr())
    {
        return traits_type::to_int_type(*gptr());
    }
    return traits_type::eof();
}

InputMemoryStream::InputMemoryStream(const void* buffer, size_t size)
    : std::istream(&buffer_), buffer_(const_cast<void*>(buffer), size)
{
}

OutputMemoryStream::OutputMemoryStream(void* buffer, size_t size) : std::ostream(&buffer_), buffer_(buffer, size)
{
}

} // namespace kb