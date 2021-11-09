#pragma once

#include <string>

namespace kb
{

/**
 * @brief Get the backtrace as a string.
 * The stack trace is a representation of the call stack at this instant. It helps to visualize the succession of calls
 * that led to a particular call that produced an error, so the problem can be traced back to its source.
 * 
 * @note Only implemented for linux systems at the moment.
 * @return std::string The stack trace
 */
std::string get_backtrace();

/**
 * @brief Print the stack trace using std::cout.
 *
 */
void print_backtrace();

/**
 * @brief Print the stack trace using printf().
 *
 */
void printf_backtrace();

} // namespace kb
