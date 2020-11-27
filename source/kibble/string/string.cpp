#include "string/string.h"
#include "hash/hashstr.h"

namespace kb
{
namespace su
{

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
    size_t size = static_cast<size_t>(std::stoi(input.substr(0,delimiter_pos)));
    switch(H_(input.substr(delimiter_pos+1).c_str()))
    {
        case "B"_h:  return size;
        case "kB"_h: return size * 1024;
        case "MB"_h: return size * 1024*1024;
        case "GB"_h: return size * 1024*1024*1024;
    }
    return size;
}

std::string size_to_string(size_t size)
{
    static const std::string sizes[] = {"_B", "_kB", "_MB", "_GB"};

    int ii = 0;
    while(size%1024 == 0 && ii < 4)
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
    size_t after  = static_cast<size_t>(before + size_t(diff) % 2);
    input = std::string(before, ' ') + input + std::string(after, ' ');
}

} // namespace su
} // namespace kb