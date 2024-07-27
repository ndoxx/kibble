#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <regex>
#include <sstream>
#include <string>

#include "../hash/hash.h"

namespace kb
{

template <typename T>
std::string to_string(const T& x)
{
    return std::to_string(x);
}

namespace su
{
/**
 * @brief Trim spaces from the start (in place).
 *
 * @param s
 */
inline void ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

/**
 * @brief Trim spaces from the end (in place).
 *
 * @param s
 */
inline void rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
}

/**
 * @brief Trim spaces from both ends (in place).
 *
 * @param s
 */
inline void trim(std::string& s)
{
    ltrim(s);
    rtrim(s);
}

/**
 * @brief Trim spaces from the start (copying).
 *
 * @param s
 * @return std::string
 */
inline std::string ltrim_copy(std::string s)
{
    ltrim(s);
    return s;
}

/**
 * @brief Trim spaces from the end (copying).
 *
 * @param s
 * @return std::string
 */
inline std::string rtrim_copy(std::string s)
{
    rtrim(s);
    return s;
}

/**
 * @brief Trim spaces from both ends (copying).
 *
 * @param s
 * @return std::string
 */
inline std::string trim_copy(std::string s)
{
    trim(s);
    return s;
}

/**
 * @brief Remove all spaces.
 *
 * @param s
 */
inline void strip_spaces(std::string& s)
{
    s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); }),
            s.end());
}

/**
 * @brief Convert string to lower case.
 *
 * @param str
 */
inline void to_lower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
}

/**
 * @brief Convert string to upper case.
 *
 * @param str
 */
inline void to_upper(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::toupper(c); });
}

/**
 * @brief Tokenize an input string into a vector of strings, specifying a delimiter.
 *
 * @param str
 * @param delimiter
 * @return std::vector<std::string>
 */
std::vector<std::string> tokenize(const std::string& str, char delimiter = ',');

/**
 * @brief Tokenize an input string and call a visitor for each token.
 *
 * @param str
 * @param delimiter
 * @param visit
 */
void tokenize(const std::string& str, char delimiter, std::function<void(const std::string&)> visit);

/**
 * @brief Convert a size string to a number.
 *
 * @param input
 * @param delimiter
 * @return size_t
 */
size_t parse_size(const std::string& input, char delimiter = '_');

/**
 * @brief Convert a size number to a string.
 *
 * @see kb::memory::utils::human_size()
 * @param size
 * @return std::string
 */
std::string human_size(size_t size);

/**
 * @brief Space-pad a string left and right to make it centered.
 *
 * @param input
 * @param size
 */
void center(std::string& input, int size);

/**
 * @brief Another string tokenizer that pushes tokens into an output argument container.
 * @deprecated The tokenize() function already does the job. If this function happens to be trivially replaceable in my
 * codebase I'll get rid of it.
 *
 * @see tokenize(const std::string &, char)
 * @tparam Container container type
 * @param str the input string
 * @param cont a container that has the push_back() function
 * @param delim delimiter character
 */
template <class Container>
inline void split_string(const std::string& str, Container& cont, char delim = ' ')
{
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim))
    {
        cont.push_back(token);
    }
}

/**
 * @brief Base64-encode some data.
 *
 * @param data
 * @param size
 * @return std::string
 */
std::string base64_encode(const char* data, size_t size);

/**
 * @brief Base64-encode a string.
 *
 * @param data
 * @return std::string
 */
inline std::string base64_encode(const std::string data)
{
    return base64_encode(data.data(), data.size());
}

/**
 * @brief Decode a Base64-encoded string.
 *
 * @param data
 * @return std::string
 */
std::string base64_decode(const std::string data);

/**
 * @brief Concatenate multiple arguments of different types into a string.
 *
 * @tparam Args types of arguments
 * @param args the arguments must be serializable to a stream
 * @return the concatenation of all string representations of the arguments
 */
template <typename... Args>
std::string concat(Args&&... args)
{
    std::stringstream ss;
    (ss << ... << args);
    return ss.str();
}

/**
 * @brief Concatenate multiple arguments of different types into a string and return its hash.
 *
 * @tparam Args types of arguments
 * @param args the arguments must be serializable to a stream
 * @return hash of the concatenated string
 */
template <typename... Args>
inline hash_t h_concat(Args&&... args)
{
    return H_(concat(std::forward<Args>(args)...));
}

namespace rx
{
/**
 * @brief Regex replace with a callback.
 *
 * @tparam BidirIt bidirectional iterator type
 * @tparam Traits character traits class
 * @tparam CharT character type
 * @tparam UnaryFunction callback type
 * @param first iterator to the first element of the character collection
 * @param last iterator past the last element of the character collection
 * @param re regex object
 * @param replace functor that takes a match result as input and outputs a replacement string
 * @return a string where each regex match has been processed by the replace functor
 */
template <class BidirIt, class Traits, class CharT, class UnaryFunction>
std::basic_string<CharT> regex_replace(BidirIt first, BidirIt last, const std::basic_regex<CharT, Traits>& re,
                                       UnaryFunction replace)
{
    std::basic_string<CharT> ret;

    typename std::match_results<BidirIt>::difference_type last_match_pos = 0;
    auto last_match_end = first;

    auto callback = [&](const std::match_results<BidirIt>& match) {
        auto current_match_pos = match.position(0);
        auto diff = current_match_pos - last_match_pos;

        auto current_match_start = last_match_end;
        std::advance(current_match_start, diff);

        ret.append(last_match_end, current_match_start);
        ret.append(replace(match));

        auto match_length = match.length(0);

        last_match_pos = current_match_pos + match_length;

        last_match_end = current_match_start;
        std::advance(last_match_end, match_length);
    };

    std::regex_iterator<BidirIt> begin(first, last, re), end;
    std::for_each(begin, end, callback);

    ret.append(last_match_end, last);

    return ret;
}

/**
 * @brief Convenience function to regex_replace() in a string.
 *
 * @see regex_replace()
 * @tparam Traits character traits class
 * @tparam CharT character type
 * @tparam UnaryFunction callback type
 * @param str input string
 * @param re regex object
 * @param replace functor that takes a match result as input and outputs a replacement string
 * @return a string where each regex match has been processed by the replace functor
 */
template <class Traits, class CharT, class UnaryFunction>
std::string regex_replace(const std::string& str, const std::basic_regex<CharT, Traits>& re, UnaryFunction replace)
{
    return regex_replace(str.cbegin(), str.cend(), re, replace);
}

} // namespace rx
} // namespace su
} // namespace kb
