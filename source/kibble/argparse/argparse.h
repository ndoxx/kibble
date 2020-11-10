#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

/* FEATURES:
 * - Simple and natural interface
 * - Single/double dash syntax, multiple flags can be concatenated after a single dash
 * - Handles optional flags and variables as well as required positional arguments
 * - Variables and positionals can have integer, floating point and string operands
 * - Exclusive flags and variables
 * - Automatic usage and version strings generation
 * - Useful error messages on parsing error
 *
 *
 * TODO:
 * - Argument dependency
 * 		-> An optional argument may require another one to be set
 * - List operands
 *      -> An optional argument may expect a comma separated list of values
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

enum class ArgType : uint8_t
{
    NONE,
    BOOL,
    INT,
    LONG,
    FLOAT,
    DOUBLE,
    STRING
};

struct AbstractArgument
{
    size_t exclusive_idx = 0;
    char dependency = 0;
    bool is_set = false;
    char short_name = 0;
    std::string full_name;
    std::string description;

    virtual ~AbstractArgument() = default;
    virtual void cast(const std::string&) noexcept(false) = 0;
    virtual ArgType underlying_type() const = 0;
};

template <typename T> struct Argument : public AbstractArgument
{
    T value;

    virtual ~Argument() = default;
    virtual void cast(const std::string&) noexcept(false) override {}
    virtual ArgType underlying_type() const override { return ArgType::NONE; }
    inline const T& operator()() const { return value; }
};

using ArgFlag = Argument<bool>;

template <typename T> struct DefaultInit
{
    static inline T from(int Z) { return T(Z); }
};

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
    const Argument<T>& add_variable(char short_name, const std::string& full_name, const std::string& description,
                                    T default_value = DefaultInit<T>::from(0))
    {
        Argument<T>* var = new Argument<T>();
        var->short_name = short_name;
        var->full_name = full_name;
        var->description = description;
        if constexpr(!std::is_same_v<T,bool>)
            var->value = default_value;
        arguments_.insert(std::pair(short_name, var));
        full_to_short_.insert(std::pair(full_name, short_name));

        return *var;
    }

    const ArgFlag& add_flag(char short_name, const std::string& full_name, const std::string& description)
    {
        return add_variable<bool>(short_name, full_name, description);
    }

    template <typename T>
    const Argument<T>& add_positional(const std::string& full_name, const std::string& description)
    {
        Argument<T>* var = new Argument<T>();
        var->full_name = full_name;
        var->description = description;
        var->value = T(0);
        positionals_.push_back(var);

        return *var;
    }

    void set_flags_exclusive(const std::set<char>& exclusive_set);
    void set_variables_exclusive(const std::set<char>& exclusive_set);
    void set_dependent(char key, char req);
    bool parse(int argc, char** argv) noexcept;

private:
    bool try_match_special_command(const std::string& arg) noexcept;
    char try_set_flag_group(const std::string& group) noexcept;
    bool try_set_positional(size_t& current_positional, const std::string& arg) noexcept(false);

    bool check_positional_requirements() noexcept;
    bool check_exclusivity_constraints() noexcept;
    std::set<char> get_active_flags() const noexcept;
    std::set<char> get_active_variables() const noexcept;
    bool check_intersection(const std::set<char> active, const std::vector<std::set<char>>& exclusives) noexcept;

    void make_usage_string();
    void make_version_string();
    inline void log_error(const std::string& err) { error_log_.push_back(err); }

private:
    std::string ver_string_;
    std::string program_name_;
    std::string usage_string_;
    std::string full_ver_string_;
    std::map<char, AbstractArgument*> arguments_;
    std::vector<AbstractArgument*> positionals_;
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

template <> struct Argument<bool> : public AbstractArgument
{
    virtual ~Argument() = default;
    virtual void cast(const std::string&) noexcept override { is_set = true; }
    virtual ArgType underlying_type() const override { return ArgType::BOOL; }
    inline bool operator()() const { return is_set; }
};

template <> struct Argument<int> : public AbstractArgument
{
    int value;

    virtual ~Argument() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stoi(arg); }
    virtual ArgType underlying_type() const override { return ArgType::INT; }
    inline int operator()() const { return value; }
};

template <> struct Argument<long> : public AbstractArgument
{
    long value;

    virtual ~Argument() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stol(arg); }
    virtual ArgType underlying_type() const override { return ArgType::LONG; }
    inline long operator()() const { return value; }
};

template <> struct Argument<float> : public AbstractArgument
{
    float value;

    virtual ~Argument() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stof(arg); }
    virtual ArgType underlying_type() const override { return ArgType::FLOAT; }
    inline float operator()() const { return value; }
};

template <> struct Argument<double> : public AbstractArgument
{
    double value;

    virtual ~Argument() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stod(arg); }
    virtual ArgType underlying_type() const override { return ArgType::DOUBLE; }
    inline double operator()() const { return value; }
};

template <> struct Argument<std::string> : public AbstractArgument
{
    std::string value;

    virtual ~Argument() = default;
    virtual void cast(const std::string& arg) noexcept override { value = arg; }
    virtual ArgType underlying_type() const override { return ArgType::STRING; }
    inline const std::string& operator()() const { return value; }
};

template <> struct DefaultInit<std::string>
{
    static inline std::string from(int) { return ""; }
};

} // namespace ap
} // namespace kb