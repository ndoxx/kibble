#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * TODO:
 * - Argument dependency
 * 		-> An optional argument may require another one to be set
 * - List operands
 *      -> An optional argument may expect a comma separated list of values
 */

namespace kb
{
namespace ap
{

struct ArgFlag
{
    size_t exclusive_idx = 0;
    bool value = false;
    char short_name = 0;
    std::string full_name;
    std::string description;

    inline bool operator()() const { return value; }
};

struct ArgVarBase
{
    size_t exclusive_idx = 0;
    bool is_set = false;
    char short_name = 0;
    std::string full_name;
    std::string description;

    virtual ~ArgVarBase() = default;
    virtual void cast(const std::string&) noexcept(false) = 0;
    virtual std::string underlying_type() const = 0;
};

template <typename T> struct ArgVar : public ArgVarBase
{
    T value;

    virtual ~ArgVar() = default;
    virtual void cast(const std::string&) noexcept(false) override {}
    virtual std::string underlying_type() const override { return "NONE"; }
};

template <typename T> struct DefaultInit
{
    static inline T from(int Z) { return T(Z); }
};

class ArgParse
{
public:
    ArgParse(const std::string& program_name, const std::string& ver_string);
    ~ArgParse();

    inline const std::string& get_program_name() const { return program_name_; }
    inline const std::string& get_version_string() const { return ver_string_; }

    template <typename T>
    const ArgVar<T>& add_variable(char short_name, const std::string& full_name, const std::string& description,
                                  T default_value = DefaultInit<T>::from(0))
    {
        ArgVar<T>* var = new ArgVar<T>();
        var->short_name = short_name;
        var->full_name = full_name;
        var->description = description;
        var->value = default_value;
        variables_.insert(std::pair(short_name, var));
        full_to_short_.insert(std::pair(full_name, short_name));

        return *var;
    }

    template <typename T> const ArgVar<T>& add_positional(const std::string& full_name, const std::string& description)
    {
        ArgVar<T>* var = new ArgVar<T>();
        var->full_name = full_name;
        var->description = description;
        var->value = T(0);
        positionals_.push_back(var);

        return *var;
    }

    const ArgFlag& add_flag(char short_name, const std::string& full_name, const std::string& description,
                            bool default_value = false);
    void set_flags_exclusive(const std::set<char>& exclusive_set);
    void set_variables_exclusive(const std::set<char>& exclusive_set);
    bool parse(int argc, char** argv) noexcept;
    bool is_set(char short_name) const;
    void usage() const;
    void version() const;

private:
    bool try_match_special_options(const std::string& arg) noexcept;
    bool try_set_flag(char key) noexcept;
    char try_set_flag_group(const std::string& group) noexcept;
    bool try_set_variable(char key, const std::string& operand) noexcept(false);
    bool try_set_positional(size_t& current_positional, const std::string& arg) noexcept(false);

    bool check_positional_requirements() const noexcept;
    bool check_exclusivity_constraints() const noexcept;
    std::set<char> get_active_flags() const noexcept;
    std::set<char> get_active_variables() const noexcept;
    bool check_intersection(const std::set<char> active, const std::vector<std::set<char>>& exclusives,
                            std::function<void(char)> show_argument) const noexcept;

private:
    std::string ver_string_;
    std::string program_name_;
    std::map<char, ArgFlag> flags_;
    std::map<char, ArgVarBase*> variables_;
    std::vector<ArgVarBase*> positionals_;
    std::vector<std::set<char>> exclusive_flags_;
    std::vector<std::set<char>> exclusive_variables_;
    std::unordered_map<std::string, char> full_to_short_;
    bool valid_state_;
    bool was_run_;
};

template <> struct ArgVar<int> : public ArgVarBase
{
    int value;

    virtual ~ArgVar() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stoi(arg); }
    virtual std::string underlying_type() const override { return "int"; }
};

template <> struct ArgVar<long> : public ArgVarBase
{
    long value;

    virtual ~ArgVar() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stol(arg); }
    virtual std::string underlying_type() const override { return "long"; }
};

template <> struct ArgVar<float> : public ArgVarBase
{
    float value;

    virtual ~ArgVar() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stof(arg); }
    virtual std::string underlying_type() const override { return "float"; }
};

template <> struct ArgVar<double> : public ArgVarBase
{
    double value;

    virtual ~ArgVar() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stod(arg); }
    virtual std::string underlying_type() const override { return "double"; }
};

template <> struct ArgVar<std::string> : public ArgVarBase
{
    std::string value;

    virtual ~ArgVar() = default;
    virtual void cast(const std::string& arg) noexcept override { value = arg; }
    virtual std::string underlying_type() const override { return "string"; }
};

template <> struct DefaultInit<std::string>
{
    static inline std::string from(int) { return ""; }
};

} // namespace ap
} // namespace kb