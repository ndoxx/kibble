#include "string/string.h"
#include "hash/hashstr.h"

namespace kb
{
namespace su
{

static constexpr char s_base64_chars[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
    's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

static constexpr int s_base64_decode_vals[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
    45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// Tokenize an input string into a vector of strings, specifying a delimiter
std::vector<std::string> tokenize(const std::string& str, char delimiter)
{
    std::vector<std::string> dst;
    std::stringstream ss(str);

    while(ss.good())
    {
        std::string substr;
        std::getline(ss, substr, delimiter);
        dst.push_back(substr);
    }
    return dst;
}

// Tokenize an input string and call a visitor for each token
void tokenize(const std::string& str, char delimiter, std::function<void(const std::string&)> visit)
{
    std::stringstream ss(str);

    while(ss.good())
    {
        std::string substr;
        std::getline(ss, substr, delimiter);
        visit(substr);
    }
}

// Convert a size string to a number
size_t parse_size(const std::string& input, char delimiter)
{
    auto delimiter_pos = input.find_first_of(delimiter);
    size_t size = static_cast<size_t>(std::stoi(input.substr(0, delimiter_pos)));
    switch(H_(input.substr(delimiter_pos + 1).c_str()))
    {
    case "B"_h:
        return size;
    case "kB"_h:
        return size * 1024;
    case "MB"_h:
        return size * 1024 * 1024;
    case "GB"_h:
        return size * 1024 * 1024 * 1024;
    }
    return size;
}

std::string size_to_string(size_t size)
{
    static const std::string sizes[] = {"_B", "_kB", "_MB", "_GB"};

    int ii = 0;
    while(size % 1024 == 0 && ii < 4)
    {
        size /= 1024;
        ++ii;
    }

    return std::to_string(size) + sizes[ii];
}

void center(std::string& input, int size)
{
    int diff = size - static_cast<int>(input.size());
    if(diff <= 0)
        return;

    size_t before = static_cast<size_t>(diff / 2);
    size_t after = static_cast<size_t>(before + size_t(diff) % 2);
    input = std::string(before, ' ') + input + std::string(after, ' ');
}

std::string base64_encode(const std::string data)
{
    std::string out;

    unsigned val = 0;
    int valb = -6;
    for(char c : data)
    {
        val = (val << 8) + static_cast<unsigned char>(c);
        valb += 8;
        while(valb >= 0)
        {
            out.push_back(s_base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if(valb > -6)
        out.push_back(s_base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while(out.size() % 4)
        out.push_back('=');
    return out;
}

std::string base64_decode(const std::string data)
{
    std::string out;

    unsigned val = 0;
    int valb = -8;
    for(char c : data)
    {
        if(s_base64_decode_vals[size_t(c)] == -1)
            break;
        val = (val << 6) + static_cast<unsigned int>(s_base64_decode_vals[size_t(c)]);
        valb += 6;
        if(valb >= 0)
        {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

} // namespace su
} // namespace kb