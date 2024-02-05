#include "filesystem/md5.h"
#include "fmt/format.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /*
        Example strings from: https://en.wikipedia.org/wiki/MD5

        The 128-bit (16-byte) MD5 hashes (also termed message digests) are typically represented
        as a sequence of 32 hexadecimal digits. The following demonstrates a
        43-byte ASCII input and the corresponding MD5 hash:

            MD5("The quick brown fox jumps over the lazy dog") =
            9e107d9d372bb6826bd81d3542a419d6

        Even a small change in the message will (with overwhelming probability) result in a mostly
        different hash, due to the avalanche effect. For example, adding a period to the end of
        the sentence:

            MD5("The quick brown fox jumps over the lazy dog.") =
            e4d909c290d0fb1ca068ffaddf22cbd0

        The hash of the zero-length string is:

            MD5("") =
            d41d8cd98f00b204e9800998ecf8427e
    */

    {
        std::string str = "The quick brown fox jumps over the lazy dog";
        kb::md5 md;
        md.process(str.data(), str.size());
        md.finish();
        fmt::print("\"{}\" -> {}\n", str, md.to_string());
    }

    {
        std::string str = "The quick brown fox jumps over the lazy dog.";
        kb::md5 md;
        md.process(str.data(), str.size());
        md.finish();
        fmt::print("\"{}\" -> {}\n", str, md.to_string());
    }

    {
        // We can calculate the hash in one go using the specialized constructor
        std::string str = "";
        kb::md5 md(str.data(), str.size());
        fmt::print("\"{}\" -> {}\n", str, md.to_string());
    }

    // Here we show that processing the string in one go and feeding it progressively give the same result
    size_t N = 13;

    {
        std::string str;
        for (size_t ii = 0; ii < N; ++ii)
            str += "0123456789";

        kb::md5 md;
        md.process(str.data(), str.size());
        md.finish();
        fmt::print("{}\n", md.to_string());
    }

    {
        std::string str = "0123456789";
        kb::md5 md;
        for (size_t ii = 0; ii < N; ++ii)
        {
            md.process(str.data(), str.size());
        }
        md.finish();
        fmt::print("{}\n", md.to_string());
    }

    return 0;
}