#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

/*
 * TODO:
 * - Exclusive flags sets
 * 		-> No two flags of the same set shall be active simultaneously
 * - Exclusive variables sets
 * 		-> No two variables of the same set shall be set simultaneously
 * - Argument dependency
 * 		-> An optional argument may require another one to be set
 */

namespace kb
{
namespace ap
{

struct ArgFlag
{
    size_t exclusive_idx = 0;
    bool value;
    char short_name;
    std::string full_name;
    std::string description;
};

struct ArgVarBase
{
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

template <> struct ArgVar<int> : public ArgVarBase
{
    int value;

    virtual ~ArgVar() = default;
    virtual void cast(const std::string& arg) noexcept(false) override { value = std::stoi(arg); }
    virtual std::string underlying_type() const override { return "int"; }
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
                                  T default_value = T(0))
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

    void add_flag(char short_name, const std::string& full_name, const std::string& description,
                  bool default_value = false);
    void set_flags_exclusive(const std::set<char>& exclusive_set);
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
    bool check_positional_requirements() noexcept;
    bool check_exclusivity_constraints() noexcept;

private:
    std::string ver_string_;
    std::string program_name_;
    std::map<char, ArgFlag> flags_;
    std::map<char, ArgVarBase*> variables_;
    std::vector<ArgVarBase*> positionals_;
    std::vector<std::set<char>> exclusive_flags_;
    std::unordered_map<std::string, char> full_to_short_;
    bool valid_state_;
    bool was_run_;
    int argc_;
};

} // namespace ap
} // namespace kb