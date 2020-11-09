#pragma once

#include <regex>
#include <string>
#include <vector>

namespace tc
{

std::vector<std::string> tokenize(const char* data, size_t size)
{
    std::string str(data, size);
    std::regex reg("\\s+");
    std::sregex_token_iterator iter(str.begin(), str.end(), reg, -1);
    std::sregex_token_iterator end;
    std::vector<std::string> result(iter, end);

    return result;
}

inline std::vector<std::string> tokenize(const std::string& str) { return tokenize(str.data(), str.size()); }

std::vector<const char*> make_argv(const std::vector<std::string>& arguments)
{
    std::vector<const char*> argv;
    for(const auto& arg : arguments)
        argv.push_back(static_cast<const char*>(arg.data()));
    argv.push_back(nullptr);
    return argv;
}

} // namespace tc