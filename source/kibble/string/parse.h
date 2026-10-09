#pragma once

#include <string_view>

namespace kb::su
{

/// @brief Remove leading and trailing whitespace (space, tab, CR, LF). Returns a view of the same data.
std::string_view trim_sv(std::string_view str);

/**
 * @brief Parse a string into a double.
 *
 * The grammar is: [+-] ( digits [ '.' [digits] ] | '.' digits ) [ (e|E) [+-] digits ]
 * The whole token must match the grammar. The token must not contain spaces.
 * The result is the double that is nearest to the text. The function does not allocate memory.
 *
 * @param token The text to parse.
 * @param result [out] The parsed value. The function changes it only if it returns true.
 * @return true if the token is a valid number. false if the syntax is not valid or the value is too large.
 *
 * @note A value that is too small gives zero or a subnormal number. This is not an error.
 * @note The text "-0" gives negative zero.
 *
 * @par Example:
 * @code
 * double value;
 * if (parse_double("1.5e-10", value))
 * {
 *     // value is 1.5e-10
 * }
 * @endcode
 */
bool parse_double(std::string_view token, double& result);

/**
 * @brief Parse a string into a float.
 *
 * The accepted text is the same as for parse_double(). The function parses a double and then converts it to float.
 * The result is not always the float that is nearest to the text. A value outside the float range gives infinity.
 *
 * @param token The text to parse.
 * @param result [out] The parsed value. The value is not specified if the function returns false.
 * @return true if the token is a valid number, false otherwise.
 *
 * @see parse_double()
 */
inline bool parse_float(std::string_view token, float& result)
{
    double result_double{0.0};
    bool success = parse_double(token, result_double);
    result = static_cast<float>(result_double);
    return success;
}

} // namespace kb::su