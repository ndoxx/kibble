#pragma once

#include <string_view>

namespace kb::su
{

/**
 * @brief Trim whitespace characters from both ends of a string view
 *
 * Removes leading and trailing whitespace characters (space, tab, carriage return, newline)
 * from the given string view without modifying the original data or performing any allocations.
 *
 * @param str The string view to trim
 * @return std::string_view A new string view referencing the trimmed portion of the original data.
 *         Returns an empty string view if the input contains only whitespace or is empty.
 *
 * @note This function performs no memory allocations and operates in O(n) time complexity
 *       where n is the length of the input string view.
 *
 * @par Example:
 * @code
 * std::string_view input = "  hello world  \n";
 * std::string_view trimmed = trim_sv(input);
 * // trimmed now references "hello world"
 * @endcode
 */
std::string_view trim_sv(std::string_view str);

/**
 * @brief Parse a string view into a double-precision floating-point number
 *
 * Converts a string representation of a number into a double value without performing
 * any heap allocations. Supports standard decimal notation, scientific notation (e/E),
 * and signed numbers.
 *
 * @param token The string view containing the number to parse
 * @param result [out] Reference to store the parsed double value. Only modified on successful parsing.
 * @return true if parsing succeeded and the entire token was consumed, false otherwise
 *
 * @par Supported formats:
 * - Integer: "42", "-123", "+456"
 * - Decimal: "3.14", "-2.718", ".5", "42."
 * - Scientific: "1e10", "2.5E-3", "-1.23e+4"
 *
 * @par Error conditions:
 * - Empty or whitespace-only input
 * - Invalid characters (letters, multiple decimal points, etc.)
 * - Malformed scientific notation
 * - Numbers that don't consume the entire token
 *
 * @note This function is allocation-free and designed for high-performance parsing
 *       of numerical data in text file formats.
 *
 * @warning The function expects the entire token to represent a valid number.
 *          Partial matches (e.g., "123abc") will return false.
 *
 * @par Example:
 * @code
 * double value;
 * if (parse_double("3.14159", value)) {
 *     // value now contains 3.14159
 * }
 * if (parse_double("1.5e-10", value)) {
 *     // value now contains 1.5e-10
 * }
 * @endcode
 */
bool parse_double(std::string_view token, double& result);

/**
 * @brief Parse a string view into a single-precision floating-point number
 *
 * Convenience wrapper around parse_double() that converts the result to float precision.
 * Provides the same parsing capabilities as parse_double() but returns a float value.
 *
 * @param token The string view containing the number to parse
 * @param result [out] Reference to store the parsed float value. Only modified on successful parsing.
 * @return true if parsing succeeded and the entire token was consumed, false otherwise
 *
 * @par Supported formats:
 * Same as parse_double(): integers, decimals, and scientific notation with optional signs.
 *
 * @note This function internally uses double-precision parsing and then casts to float,
 *       which may result in precision loss for very large or very precise numbers.
 *
 * @note This function is marked inline for performance in tight parsing loops.
 *
 * @see parse_double() for detailed format specifications and error conditions
 *
 * @par Example:
 * @code
 * float rgb_value;
 * if (parse_float("0.625", rgb_value)) {
 *     // rgb_value now contains 0.625f
 * }
 * @endcode
 */
inline bool parse_float(std::string_view token, float& result)
{
    double result_double{0.0};
    bool success = parse_double(token, result_double);
    result = static_cast<float>(result_double);
    return success;
}

} // namespace kb::su