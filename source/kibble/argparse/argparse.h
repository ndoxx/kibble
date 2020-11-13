#pragma once

#include <functional>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

/* FEATURES:
 * - Simple and natural interface
 * - Single/double dash syntax, multiple flags can be concatenated after a single dash
 * - Handles optional flags and variables as well as required positional arguments
 * - Options and positionals can have integer, floating point and string operands,
 *   options can also receive comma-separated lists of these types
 * - Options can be mutually exclusive or dependent on one another
 * - Automatic usage and version strings generation
 * - Useful error messages on parsing failure
 *
 *
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

struct InvalidOperandException;

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

template <typename T> T StringCast(const std::string&) noexcept(false);
template <typename T> static ArgType k_underlying_type;

struct AbstractOption
{
    char dependency = 0;
    bool is_set = false;
    char short_name = 0;
    std::string full_name;
    std::string description;
    std::set<size_t> exclusive_sets;

    virtual ~AbstractOption() = default;
    virtual void cast(const std::string&) noexcept(false) = 0;
    virtual ArgType underlying_type() const = 0;
    void format_description(std::ostream&, long max_pad) const;
};

template <typename T> struct Option : public AbstractOption
{
    T value;

    virtual ~Option() = default;
    virtual void cast(const std::string& operand) noexcept(false) override { value = StringCast<T>(operand); }
    virtual ArgType underlying_type() const override { return k_underlying_type<T>; }
    inline const T& operator()() const { return value; }
};

using Flag = Option<bool>;

class ArgParse
{
public:
    ArgParse(const std::string& program_name, const std::string& ver_string);
    ~ArgParse();

    inline const std::string& version()
    {
        if(full_ver_string_.size() == 0)
            make_version_string();
        return full_ver_string_;
    }

    inline const std::string& usage()
    {
        if(usage_string_.size() == 0)
            make_usage_string();
        return usage_string_;
    }

    inline const std::vector<std::string>& get_errors() const { return error_log_; }
    inline void set_usage_padding(long padding) { usage_padding_ = padding; }
    inline void set_log_output(std::function<void(const std::string&)> output) { output_ = output; }
    inline void set_exit_on_special_command(bool value) { exit_on_special_command_ = value; }

    template <typename T>
    const Option<T>& add_variable(char short_name, const std::string& full_name, const std::string& description,
                                  T default_value)
    {
        Option<T>* opt = new Option<T>();
        opt->short_name = short_name;
        opt->full_name = full_name;
        opt->description = description;
        if constexpr(!std::is_same_v<T, bool>)
            opt->value = default_value;
        arguments_.insert(std::pair(short_name, opt));
        full_to_short_.insert(std::pair(full_name, short_name));

        return *opt;
    }

    inline const Flag& add_flag(char short_name, const std::string& full_name, const std::string& description)
    {
        return add_variable<bool>(short_name, full_name, description, false);
    }

    template <typename T>
    inline const Option<std::vector<T>>& add_list(char short_name, const std::string& full_name,
                                                  const std::string& description)
    {
        return add_variable<std::vector<T>>(short_name, full_name, description, {});
    }

    template <typename T> const Option<T>& add_positional(const std::string& full_name, const std::string& description)
    {
        Option<T>* opt = new Option<T>();
        opt->full_name = full_name;
        opt->description = description;
        positionals_.push_back(opt);

        return *opt;
    }

    void set_flags_exclusive(const std::set<char>& exclusive_set);
    void set_variables_exclusive(const std::set<char>& exclusive_set);
    void set_dependency(char key, char req);
    bool parse(int argc, char** argv) noexcept;

private:
    inline void set_trigger(char key, std::function<void()> trig) { triggers_.insert(std::pair(key, trig)); }

    char try_set_flag_group(const std::string& group) noexcept;
    bool try_set_positional(size_t& current_positional, const std::string& arg) noexcept(false);

    bool check_positional_requirements() noexcept;
    bool check_exclusivity_constraints() noexcept;
    bool check_dependencies() noexcept;
    bool check_intersection(const std::set<char> active, const std::vector<std::set<char>>& exclusives) noexcept;
    std::set<char> get_active(std::function<bool(AbstractOption*)>) const noexcept;
    bool compatible(char, char) const;

    void make_usage_string();
    void make_version_string();
    inline void log_error(const std::string& err) { error_log_.push_back(err); }

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

template <>[[maybe_unused]] static ArgType k_underlying_type<bool> = ArgType::BOOL;
template <>[[maybe_unused]] static ArgType k_underlying_type<int> = ArgType::INT;
template <>[[maybe_unused]] static ArgType k_underlying_type<long> = ArgType::LONG;
template <>[[maybe_unused]] static ArgType k_underlying_type<float> = ArgType::FLOAT;
template <>[[maybe_unused]] static ArgType k_underlying_type<double> = ArgType::DOUBLE;
template <>[[maybe_unused]] static ArgType k_underlying_type<std::string> = ArgType::STRING;
template <>[[maybe_unused]] static ArgType k_underlying_type<std::vector<int>> = ArgType::VEC_INT;
template <>[[maybe_unused]] static ArgType k_underlying_type<std::vector<long>> = ArgType::VEC_LONG;
template <>[[maybe_unused]] static ArgType k_underlying_type<std::vector<float>> = ArgType::VEC_FLOAT;
template <>[[maybe_unused]] static ArgType k_underlying_type<std::vector<double>> = ArgType::VEC_DOUBLE;
template <>[[maybe_unused]] static ArgType k_underlying_type<std::vector<std::string>> = ArgType::VEC_STRING;

template <> bool StringCast<bool>(const std::string&) noexcept(false);
template <> int StringCast<int>(const std::string&) noexcept(false);
template <> long StringCast<long>(const std::string&) noexcept(false);
template <> float StringCast<float>(const std::string&) noexcept(false);
template <> double StringCast<double>(const std::string&) noexcept(false);
template <> std::string StringCast<std::string>(const std::string&) noexcept(false);
template <> std::vector<int> StringCast<std::vector<int>>(const std::string&) noexcept(false);
template <> std::vector<long> StringCast<std::vector<long>>(const std::string&) noexcept(false);
template <> std::vector<float> StringCast<std::vector<float>>(const std::string&) noexcept(false);
template <> std::vector<double> StringCast<std::vector<double>>(const std::string&) noexcept(false);
template <> std::vector<std::string> StringCast<std::vector<std::string>>(const std::string&) noexcept(false);

template <> struct Option<bool> : public AbstractOption
{
    virtual ~Option() = default;
    virtual void cast(const std::string&) noexcept(false) override { is_set = true; }
    virtual ArgType underlying_type() const override { return ArgType::BOOL; }
    inline bool operator()() const { return is_set; }
};

} // namespace ap
} // namespace kb