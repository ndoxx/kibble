#include "kibble/string/parse.h"

namespace kb::su
{

std::string_view trim_sv(std::string_view str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return {};
    }

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool parse_double(std::string_view token, double& result)
{
    if (token.empty())
    {
        return false;
    }

    const char* start = token.data();
    const char* end = start + token.size();
    const char* ptr = start;

    // Handle sign
    bool negative = false;
    if (ptr < end && *ptr == '-')
    {
        negative = true;
        ++ptr;
    }
    else if (ptr < end && *ptr == '+')
    {
        ++ptr;
    }

    // Parse integer part
    double value = 0.0;
    bool found_digits = false;
    while (ptr < end && *ptr >= '0' && *ptr <= '9')
    {
        value = value * 10.0 + (*ptr - '0');
        ++ptr;
        found_digits = true;
    }

    // Parse fractional part
    if (ptr < end && *ptr == '.')
    {
        ++ptr;
        double decimal_place = 0.1;
        while (ptr < end && *ptr >= '0' && *ptr <= '9')
        {
            value += (*ptr - '0') * decimal_place;
            decimal_place *= 0.1;
            ++ptr;
            found_digits = true;
        }
    }

    // Handle scientific notation (e/E)
    if (ptr < end && (*ptr == 'e' || *ptr == 'E'))
    {
        ++ptr;
        bool exp_negative = false;
        if (ptr < end && *ptr == '-')
        {
            exp_negative = true;
            ++ptr;
        }
        else if (ptr < end && *ptr == '+')
        {
            ++ptr;
        }

        int exponent = 0;
        bool found_exp_digits = false;
        while (ptr < end && *ptr >= '0' && *ptr <= '9')
        {
            exponent = exponent * 10 + (*ptr - '0');
            ++ptr;
            found_exp_digits = true;
        }

        if (!found_exp_digits)
        {
            return false;
        }

        if (exp_negative)
        {
            exponent = -exponent;
        }

        // Apply exponent
        for (int i = 0; i < exponent; ++i)
        {
            value *= 10.0;
        }
        for (int i = 0; i < -exponent; ++i)
        {
            value *= 0.1;
        }
    }

    // Check if we consumed the entire token and found at least one digit
    if (ptr != end || !found_digits)
    {
        return false;
    }

    result = negative ? -value : value;
    return true;
};

} // namespace kb::su