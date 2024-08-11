#pragma once

#include <iostream>
#include <vector>

namespace kb
{

/**
 * @brief Custom ifstream-like input stream to access individual files in a PackFile
 * 
 */
class PackFileStream : public std::istream
{
public:
    PackFileStream(std::istream& base_stream, std::streampos start, std::streamsize size);

private:
    class PackFileStreamBuf : public std::streambuf
    {
    public:
        PackFileStreamBuf(std::istream& base, std::streampos start, std::streamsize size,
                          std::streamsize bufferSize = 1024);

    protected:
        virtual int_type underflow() override;

        virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                                 std::ios_base::openmode which = std::ios_base::in) override;

        virtual pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in) override;

    private:
        std::istream& base_stream_;
        std::streampos start_;
        std::streamsize size_;
        std::vector<char> buffer_;
    };

private:
    PackFileStreamBuf buffer_;
};

} // namespace kb