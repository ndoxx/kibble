#include "kibble/string/string.h"
#include "kibble/hash/hash.h"

#include "fmt/core.h"

namespace kb
{
namespace su
{



// Tokenize an input string into a vector of strings, specifying a delimiter
std::vector<std::string> tokenize(const std::string& str, char delimiter)
{
    std::vector<std::string> dst;
    std::stringstream ss(str);

    while (ss.good())
    {
        std::string substr;
        std::getline(ss, substr, delimiter);
        dst.push_back(substr);
    }
    return dst;
}

// Tokenize an input string and call a visitor for each token
void tokenize(const std::string& str, char delimiter, const std::function<void(const std::string&)>& visit)
{
    std::stringstream ss(str);

    while (ss.good())
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
    switch (H_(input.substr(delimiter_pos + 1).c_str()))
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

constexpr const char* k_size_unit_suffix[] = {
    "B", "kB", "MB", "GB", "TB", "??",
};

constexpr int k_max_suffix = 4;

std::string human_size(std::size_t bytes)
{
    int ii = 0;
    double d_bytes = double(bytes);

    if (bytes > 1024)
    {
        for (ii = 0; (bytes / 1024) > 0 && ii < k_max_suffix; ii++, bytes /= 1024)
        {
            d_bytes = double(bytes) / 1024.0;
        }
    }

    return fmt::format("{:.2f}{}", d_bytes, k_size_unit_suffix[ii]);
}

void center(std::string& input, int size)
{
    int diff = size - static_cast<int>(input.size());
    if (diff <= 0)
    {
        return;
    }

    size_t before = static_cast<size_t>(diff / 2);
    size_t after = before + size_t(diff) % 2;
    input = std::string(before, ' ') + input + std::string(after, ' ');
}

void collapse(std::string& input, char target)
{
    // Use iterators to track read and write positions
    auto read = input.begin();
    auto write = input.begin();

    // Loop through characters
    for (char c : input)
    {
        // If not a repeated underscore, copy and advance both iterators
        if (c != '_' || (read != input.begin() && *(read - 1) != target))
        {
            *write = c;
            ++write;
        }
        ++read;
    }

    // Truncate the string to the write position
    input = std::string(input.begin(), write);
}

} // namespace su
} // namespace kb