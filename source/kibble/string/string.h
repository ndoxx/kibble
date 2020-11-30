#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <locale>
#include <regex>
#include <sstream>
#include <string>

namespace kb
{

template <typename T> std::string to_string(const T& x) { return std::to_string(x); }

namespace su
{
// Trim from start (in place)
[[maybe_unused]] static inline void ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

// Trim from end (in place)
[[maybe_unused]] static inline void rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
}

// Trim from both ends (in place)
[[maybe_unused]] static inline void trim(std::string& s)
{
    ltrim(s);
    rtrim(s);
}

// Trim from start (copying)
[[maybe_unused]] static inline std::string ltrim_copy(std::string s)
{
    ltrim(s);
    return s;
}

// Trim from end (copying)
[[maybe_unused]] static inline std::string rtrim_copy(std::string s)
{
    rtrim(s);
    return s;
}

// Trim from both ends (copying)
[[maybe_unused]] static inline std::string trim_copy(std::string s)
{
    trim(s);
    return s;
}

[[maybe_unused]] static inline void to_lower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
}

[[maybe_unused]] static inline void to_upper(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::toupper(c); });
}

// Tokenize an input string into a vector of strings, specifying a delimiter
std::vector<std::string> tokenize(const std::string& str, char delimiter = ',');

// Tokenize an input string and call a visitor for each token
void tokenize(const std::string& str, char delimiter, std::function<void(const std::string&)> visit);

// Convert a size string to a number
size_t parse_size(const std::string& input, char delimiter = '_');

// Convert a size number to a string
std::string size_to_string(size_t size);

// Space-pad a string left and right to make it centered
void center(std::string& input, int size);

template <class Container> static inline void split_string(const std::string& str, Container& cont, char delim = ' ')
{
    std::stringstream ss(str);
    std::string token;
    while(std::getline(ss, token, delim))
    {
        cont.push_back(token);
    }
}

// Base64-encode a string
std::string base64_encode(const std::string data);

namespace rx
{
// Regex replace with a callback
template <class BidirIt, class Traits, class CharT, class UnaryFunction>
std::basic_string<CharT> regex_replace(BidirIt first, BidirIt last, const std::basic_regex<CharT, Traits>& re,
                                       UnaryFunction f)
{
    std::basic_string<CharT> s;

    typename std::match_results<BidirIt>::difference_type positionOfLastMatch = 0;
    auto endOfLastMatch = first;

    auto callback = [&](const std::match_results<BidirIt>& match) {
        auto positionOfThisMatch = match.position(0);
        auto diff = positionOfThisMatch - positionOfLastMatch;

        auto startOfThisMatch = endOfLastMatch;
        std::advance(startOfThisMatch, diff);

        s.append(endOfLastMatch, startOfThisMatch);
        s.append(f(match));

        auto lengthOfMatch = match.length(0);

        positionOfLastMatch = positionOfThisMatch + lengthOfMatch;

        endOfLastMatch = startOfThisMatch;
        std::advance(endOfLastMatch, lengthOfMatch);
    };

    std::regex_iterator<BidirIt> begin(first, last, re), end;
    std::for_each(begin, end, callback);

    s.append(endOfLastMatch, last);

    return s;
}

template <class Traits, class CharT, class UnaryFunction>
std::string regex_replace(const std::string& s, const std::basic_regex<CharT, Traits>& re, UnaryFunction f)
{
    return regex_replace(s.cbegin(), s.cend(), re, f);
}

} // namespace rx
} // namespace su
} // namespace kb
