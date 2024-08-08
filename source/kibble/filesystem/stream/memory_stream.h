#pragma once

#include <cstring>
#include <istream>
#include <ostream>

namespace kb
{

class MemoryBuffer : public std::streambuf
{
public:
    MemoryBuffer(void* buffer, size_t size);

    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override;
    pos_type seekpos(pos_type pos, std::ios_base::openmode which) override;

protected:
    std::streamsize xsgetn(char* s, std::streamsize n) override;
    std::streamsize xsputn(const char* s, std::streamsize n) override;
    int_type overflow(int_type ch = traits_type::eof()) override;
    int_type underflow() override;

protected:
    char* begin_;
    char* end_;
};

class InputMemoryStream : public std::istream
{
public:
    InputMemoryStream(const void* buffer, size_t size);

private:
    MemoryBuffer buffer_;
};

class OutputMemoryStream : public std::ostream
{
public:
    OutputMemoryStream(void* buffer, size_t size);

private:
    MemoryBuffer buffer_;
};

} // namespace kb