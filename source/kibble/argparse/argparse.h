/**
 * @file argparse.h
 * @author ndx
 * @brief A modern and simple to use program argument parser
 * @version 0.1
 * @date 2021-11-05
 *
 * @copyright Copyright (c) 2021
 *
 */

#pragma once

#include <functional>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * TODO:
 * - "program @arguments.txt" syntax to allow loading of arguments from a text file
 *
 * EVAL:
 * - Same option can appear multiple times
 * - Detect "--long-arg=2" and parse it like "--long-arg 2"
 * - Detect "-p2" and "-p=2" and parse them like "-p 2"
 */

namespace kb
{
namespace ap
{

/**
 * @brief Throw when a bad string cast occurs when parsing an argument
 */
struct InvalidOperandException;

/**
 * @brief Possible types a string argument can be cast to
 */
enum class ArgType : uint8_t
{
    NONE,
    BOOL,
    INT,
    LONG,
    FLOAT,
    DOUBLE,
    STRING,
    VEC_INT,
    VEC_LONG,
    VEC_FLOAT,
    VEC_DOUBLE,
    VEC_STRING
};

namespace detail
{
// Tempate function to cast a string to any handled type
template <typename T>
T StringCast(const std::string&) noexcept(false);

// Template variable to associate types to type tags
template <typename T>
static constexpr ArgType k_underlying_type = ArgType::NONE;
} // namespace detail

/**
 * @brief Represents a command line option of any type.
 * This base class is used for type erasure by the argument parser.
 */
struct AbstractOption
{
    /// Single letter form of this option
    char short_name = 0;
    /// Short name of another option that is required for this one to make sense
    char dependency = 0;
    /// True if this option was set
    bool is_set = false;
    /// Double-dash full form of this option
    std::string full_name;
    /// Small text describing what the option does
    std::string description;
    /// Compatibility requirements for this option
    std::set<size_t> exclusive_sets;

    virtual ~AbstractOption() = default;

    /**
     * @brief Initialize value from string
     */
    virtual void cast(const std::string&) noexcept(false) = 0;

    /**
     * @brief Get underlying type as a tag
     * @return ArgType
     */
    virtual ArgType underlying_type() const = 0;

    /**
     * @brief Serialize an option description line to a stream.
     * Used by the usage string generator.
     * @param max_pad Maximum padding length between the option name and its description
     */
    void format_description(std::ostream&, long max_pad) const;
};

/**
 * @brief Concrete option with an associated value of type T.
 * A flag is just an option with a boolean value.
 * @tparam T Underlying value type
 */
template <typename T>
struct Option : public AbstractOption
{
    T value; /// The value of this option

    /**
     * @brief Cast the input string to the type T and initialize value.
     *
     * @param operand String next to the option to be cast to a value
     */
    void cast(const std::string& operand) noexcept(false) override
    {
        value = detail::StringCast<T>(operand);
    }

    /**
     * @brief Get underlying type as a tag.
     *
     * @return ArgType
     */
    ArgType underlying_type() const override
    {
        return detail::k_underlying_type<T>;
    }

    /**
     * @brief Convenience operator to obtain the stored value.
     *
     * @return const T&
     */
    inline const T& operator()() const
    {
        return value;
    }
};

using Flag = Option<bool>;

/**
 * @brief Program argument parser
 *
 * FEATURES:\n
 * - Simple and natural interface
 * - Single/double dash syntax, multiple flags can be concatenated after a single dash
 * - Handles optional flags and variables as well as required positional arguments
 * - Options and positionals can have integer, floating point and string operands,
 *   options can also receive comma-separated lists of these types
 * - Options can be mutually exclusive or dependent on one another
 * - Automatic usage and version strings generation
 * - Useful error messages on parsing failure
 *
 */
class ArgParse
{
public:
    /**
     * @brief Construct a new argument parser.
     *
     * @param program_name Name of the program to display in the usage string
     * @param ver_string Version string to display when the program is called with the -v or --version flag
     */
    ArgParse(const std::string& program_name, const std::string& ver_string);
    ~ArgParse();

    /**
     * @brief Get the full version string.
     *
     * @return const std::string&
     */
    inline const std::string& version()
    {
        if (full_ver_string_.size() == 0)
            make_version_string();
        return full_ver_string_;
    }

    /**
     * @brief Get the usage string.
     * The usage string is the text displayed when the program is called with the -h or --help flag.
     * It consists of a formal string referencing the program name and all the possible options and
     * positional arguments, with their dependencies and mutual exclusions, followed by the descriptions
     * of all the options.
     * A usage string is also typically displayed on program argument parsing error, to
     * show the user the appropriate usage of the program.
     *
     * @return const std::string&
     */
    inline const std::string& usage()
    {
        if (usage_string_.size() == 0)
            make_usage_string();
        return usage_string_;
    }

    /**
     * @brief Get the list of errors that occurred during parsing
     *
     * @return const std::vector<std::string>&
     */
    inline const std::vector<std::string>& get_errors() const
    {
        return error_log_;
    }

    /**
     * @brief Set the padding length used by the usage string generator.
     * An option description line states the short and long form of an option, followed by
     * the type and a possible dependency in the left column, and the option description on
     * the right column. Between these two columns lie padding spaces.
     *
     * @param padding Maximum padding
     */
    inline void set_usage_padding(long padding)
    {
        usage_padding_ = padding;
    }

    /**
     * @brief Set the logging function.
     * This function is called by special triggers when the options -v / --version or
     * -h / --help are encountered, so the relevant text can be displayed. The logging
     * function is expected to print this information somewhere, usually the standard output.
     *
     * @param output
     */
    inline void set_log_output(std::function<void(const std::string&)> output)
    {
        output_ = output;
    }

    /**
     * @brief Allow exiting when encountering a special trigger.
     * Usually the expected behavior after encountering -h or -v is to ignore everything else, display the relevant
     * textual information and exit immediately. This behavior is enabled or disabled by calling this
     * function.
     *
     * @param value
     */
    inline void set_exit_on_special_command(bool value)
    {
        exit_on_special_command_ = value;
    }

    /**
     * @brief Add a T-valued option that expects an operand on its right.
     * The operand must be convertible to the type T or the parser will fail with an error.
     *
     * @tparam T Underlying type of the variable. It can be an int, a long, a float, a double or a string
     * @param short_name One-letter single-dash short form of this variable
     * @param full_name Multiple-letters double-dash full form of this variable
     * @param description Short text that describes what this variable does
     * @param default_value Default value this variable gets if it was not initialized in the command line
     * @return const Option<T>& An option object that can be evaluated after parsing
     */
    template <typename T>
    const Option<T>& add_variable(char short_name, const std::string& full_name, const std::string& description,
                                  T default_value)
    {
        Option<T>* opt = new Option<T>();
        opt->short_name = short_name;
        opt->full_name = full_name;
        opt->description = description;
        if constexpr (!std::is_same_v<T, bool>)
            opt->value = default_value;
        arguments_.insert(std::pair(short_name, opt));
        full_to_short_.insert(std::pair(full_name, short_name));

        return *opt;
    }

    /**
     * @brief Add a bool-valued option that will be set to true when parsed or false otherwise.
     *
     * @param short_name One-letter single-dash short form of this flag
     * @param full_name Multiple-letters double-dash full form of this flag
     * @param description Short text that describes what this flag does
     * @return const Flag& A flag object that can be evaluated after parsing
     */
    inline const Flag& add_flag(char short_name, const std::string& full_name, const std::string& description)
    {
        return add_variable<bool>(short_name, full_name, description, false);
    }

    /**
     * @brief Add a vector-valued option that expects a comma-separated list of operands on its right
     *
     * @tparam T Underlying type of all the elements of the list
     * @param short_name One-letter single-dash short form of this option
     * @param full_name Multiple-letters double-dash full form of this option
     * @param description Short text that describes what this option does
     * @return const Option<std::vector<T>>&
     */
    template <typename T>
    inline const Option<std::vector<T>>& add_list(char short_name, const std::string& full_name,
                                                  const std::string& description)
    {
        return add_variable<std::vector<T>>(short_name, full_name, description, {});
    }

    /**
     * @brief Add an argument that is always required for the program to work, and that will
     * need to be included in the proper order.
     * The n-th positional argument needs to be listed n-th when the program is called.
     * The positional arguments order is determined by the order of the calls to this function.
     * The precise number of positional arguments must be supplied to the program, or the parser
     * will fail with an error.
     *
     * @tparam T Underlying type of this argument
     * @param full_name Full name, usually capitalized, only used in the usage string
     * to make precise the role of this argument
     * @param description Short text that describes what this argument does
     * @return const Option<T>&
     */
    template <typename T>
    const Option<T>& add_positional(const std::string& full_name, const std::string& description)
    {
        Option<T>* opt = new Option<T>();
        opt->full_name = full_name;
        opt->description = description;
        positionals_.push_back(opt);

        return *opt;
    }

    /**
     * @brief Set all the flags in the input set to be mutually exclusive.
     * If two mutually exclusive flags are encountered, the parsing will fail with an error.
     * It is possible to configure multiple exclusive sets, some of which can have non-empty intersection.
     *
     * @param exclusive_set Set of short-form options to set as mutually exclusive
     */
    void set_flags_exclusive(const std::set<char>& exclusive_set);

    /**
     * @brief Set all the variables in the input set to be mutually exclusive.
     * If two mutually exclusive variables are encountered, the parsing will fail with an error.
     * It is possible to configure multiple exclusive sets, some of which can have non-empty intersection.
     *
     * @param exclusive_set Set of short-form options to set as mutually exclusive
     */
    void set_variables_exclusive(const std::set<char>& exclusive_set);

    /**
     * @brief Specify that the first command requires the second one to be present during parsing.
     * If the dependency is not satisfied, the parsing will fail with an error.
     *
     * @param key The command to set as dependant
     * @param req The command to set as required by the command key
     */
    void set_dependency(char key, char req);

    /**
     * @brief Parse the command line arguments supplied by the main() function.
     * If the parser managed to parse the arguments, it will return true. If the parser returned
     * false, some error has occurred, and even though some options may be initialized correctly,
     * it is not advised to use their values for anything. An error message should be displayed
     * with the error log's content, followed by the usage string, then the program should exit.
     *
     * @param argc Argument count
     * @param argv Argument values
     * @return true if the parser ended in a valid state
     * @return false if some error happened
     */
    bool parse(int argc, char** argv) noexcept;

private:
    // Set a special argument with immediate action
    inline void set_trigger(char key, std::function<void()> trig)
    {
        triggers_.insert(std::pair(key, trig));
    }

    // Set all flags in a concatenated flag group, return the first unknown flag if any
    char try_set_flag_group(const std::string& group) noexcept;

    // Try to parse an argument as the current positional argument
    bool try_set_positional(size_t& current_positional, const std::string& arg) noexcept(false);

    // Check that all requirements related to the positional arguments are respected
    bool check_positional_requirements() noexcept;

    // Check that no two mutually exclusive flags or variables appeared in the command line
    bool check_exclusivity_constraints() noexcept;

    // Check that all dependencies have been satisfied
    bool check_dependencies() noexcept;

    // Compute the intersection of the active set with all exclusive sets, in order to check for exclusivity constraints
    bool check_intersection(const std::set<char> active, const std::vector<std::set<char>>& exclusives) noexcept;

    // Get the set of all set options that pass the input filter
    std::set<char> get_active(std::function<bool(AbstractOption*)>) const noexcept;

    // Check if two options are compatible, meaning they don't share an exclusive set
    bool compatible(char, char) const;

    // Generate the usage string
    void make_usage_string();

    // Generate the version string
    void make_version_string();

    // Push an error string to the error log
    inline void log_error(const std::string& err)
    {
        error_log_.push_back(err);
    }

private:
    std::string ver_string_;
    std::string program_name_;
    std::string usage_string_;
    std::string full_ver_string_;
    std::map<char, AbstractOption*> arguments_;
    std::map<char, std::function<void()>> triggers_;
    std::vector<AbstractOption*> positionals_;
    std::vector<std::set<char>> exclusive_flags_;
    std::vector<std::set<char>> exclusive_variables_;
    std::unordered_map<std::string, char> full_to_short_;
    std::vector<std::string> error_log_;
    std::function<void(const std::string&)> output_ = [](const std::string&) {};

    bool valid_state_ = false;
    bool was_run_ = false;
    bool exit_on_special_command_ = true;
    long usage_padding_ = 30;
};

namespace detail
{
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<bool> = ArgType::BOOL;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<int> = ArgType::INT;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<long> = ArgType::LONG;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<float> = ArgType::FLOAT;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<double> = ArgType::DOUBLE;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<std::string> = ArgType::STRING;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<std::vector<int>> = ArgType::VEC_INT;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<std::vector<long>> = ArgType::VEC_LONG;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<std::vector<float>> = ArgType::VEC_FLOAT;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<std::vector<double>> = ArgType::VEC_DOUBLE;
template <>
[[maybe_unused]] static constexpr ArgType k_underlying_type<std::vector<std::string>> = ArgType::VEC_STRING;

template <>
bool StringCast<bool>(const std::string&) noexcept(false);
template <>
int StringCast<int>(const std::string&) noexcept(false);
template <>
long StringCast<long>(const std::string&) noexcept(false);
template <>
float StringCast<float>(const std::string&) noexcept(false);
template <>
double StringCast<double>(const std::string&) noexcept(false);
template <>
std::string StringCast<std::string>(const std::string&) noexcept(false);
template <>
std::vector<int> StringCast<std::vector<int>>(const std::string&) noexcept(false);
template <>
std::vector<long> StringCast<std::vector<long>>(const std::string&) noexcept(false);
template <>
std::vector<float> StringCast<std::vector<float>>(const std::string&) noexcept(false);
template <>
std::vector<double> StringCast<std::vector<double>>(const std::string&) noexcept(false);
template <>
std::vector<std::string> StringCast<std::vector<std::string>>(const std::string&) noexcept(false);
} // namespace detail

template <>
struct Option<bool> : public AbstractOption
{
    virtual ~Option() = default;
    virtual void cast(const std::string&) noexcept(false) override
    {
        is_set = true;
    }
    virtual ArgType underlying_type() const override
    {
        return ArgType::BOOL;
    }
    inline bool operator()() const
    {
        return is_set;
    }
};

} // namespace ap
} // namespace kb