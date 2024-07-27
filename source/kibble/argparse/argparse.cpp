#include "argparse/argparse.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <regex>

#include "fmt/core.h"
#include "fmt/format.h"

namespace kb
{
namespace ap
{

struct ParsingException : public std::exception
{
    std::string message_;
    const char* what() const noexcept override
    {
        return message_.c_str();
    }
};

struct InvalidOperandException : public ParsingException
{
    InvalidOperandException(const std::string& argument, const std::string& value)
    {
        message_.assign(fmt::format("Invalid operand: '{}' for argument: {}", value, argument));
    }
};

struct UnknownArgumentException : public ParsingException
{
    UnknownArgumentException(const std::string& argument)
    {
        message_.assign(fmt::format("Unknown argument: {}", argument));
    }
};

struct SupernumeraryArgumentException : public ParsingException
{
    SupernumeraryArgumentException(const std::string& argument)
    {
        message_.assign(fmt::format("Supernumerary argument: {}", argument));
    }
};

struct MissingOperandException : public ParsingException
{
    MissingOperandException(const std::string& argument)
    {
        message_.assign(fmt::format("Missing operand after argument: {}", argument));
    }
};

const std::unordered_map<ArgType, std::string> k_type_str = {
    {ArgType::NONE, "NONE"},
    {ArgType::BOOL, "bool"},
    {ArgType::INT, "int"},
    {ArgType::LONG, "long"},
    {ArgType::FLOAT, "float"},
    {ArgType::DOUBLE, "double"},
    {ArgType::STRING, "string"},
    {ArgType::VEC_INT, "int,..."},
    {ArgType::VEC_LONG, "long,..."},
    {ArgType::VEC_FLOAT, "float,..."},
    {ArgType::VEC_DOUBLE, "double,..."},
    {ArgType::VEC_STRING, "string,..."},
};

ArgParse::ArgParse(const std::string& program_name, const std::string& ver_string)
    : ver_string_(ver_string), program_name_(program_name)
{
    // Add special commands
    add_flag('v', "version", "Display the program version string and exit");
    add_flag('h', "help", "Display this usage string and exit");
    set_trigger('v', [this]() {
        output_(version());
        if (exit_on_special_command_)
        {
            exit(0);
        }
    });
    set_trigger('h', [this]() {
        output_(usage());
        if (exit_on_special_command_)
        {
            exit(0);
        }
    });
}

ArgParse::~ArgParse()
{
    for (auto&& [key, parg] : arguments_)
    {
        delete parg;
    }
    for (auto* parg : positionals_)
    {
        delete parg;
    }
}

void ArgParse::set_flags_exclusive(const std::set<char>& exclusive_set)
{
    for (char key : exclusive_set)
    {
        auto opt_it = arguments_.find(key);
        assert(opt_it != arguments_.end() && "Unknown flag.");
        assert(opt_it->second->underlying_type() == ArgType::BOOL && "Not a flag.");
        opt_it->second->exclusive_sets.insert(exclusive_flags_.size());
    }

    exclusive_flags_.push_back(exclusive_set);
}

void ArgParse::set_variables_exclusive(const std::set<char>& exclusive_set)
{
    for (char key : exclusive_set)
    {
        auto opt_it = arguments_.find(key);
        assert(opt_it != arguments_.end() && "Unknown variable.");
        assert(opt_it->second->underlying_type() != ArgType::BOOL && "Not a variable.");
        opt_it->second->exclusive_sets.insert(exclusive_variables_.size());
    }

    exclusive_variables_.push_back(exclusive_set);
}

bool ArgParse::parse(int argc, char** argv) noexcept
{
    assert(argc > 0 && "Arg count should be a strictly positive integer.");
    valid_state_ = true;
    was_run_ = true;
    size_t current_positional = 0;

    try
    {
        // For all tokens in argv
        for (int ii = 1; ii < argc; ++ii)
        {
            std::string arg = argv[ii];

            // Detect dash / double dash syntax
            bool single_short = (arg.size() == 2 && arg[0] == '-');
            bool single_full = (arg[0] == '-' && arg[1] == '-');
            bool positional_candidate = (arg[0] != '-');
            char key = 0;

            if (single_full)
            {
                auto full_it = full_to_short_.find(arg.substr(2));
                if (full_it != full_to_short_.end())
                {
                    key = full_it->second;
                }
                else
                {
                    throw UnknownArgumentException(arg);
                }
            }
            if (single_short)
            {
                key = arg[1];
            }

            // Single argument
            if (single_short || single_full)
            {
                // If there is a trigger associated, execute it immediately
                auto trig_it = triggers_.find(key);
                if (trig_it != triggers_.end())
                {
                    trig_it->second();
                }

                auto opt_it = arguments_.find(key);
                if (opt_it != arguments_.end())
                {
                    opt_it->second->is_set = true;
                    if (opt_it->second->underlying_type() != ArgType::BOOL)
                    {
                        // If current token is the last one, operand is missing
                        if (ii == argc - 1)
                        {
                            throw MissingOperandException(arg);
                        }

                        try
                        {
                            opt_it->second->cast(argv[ii + 1]);
                        }
                        catch (std::invalid_argument& e)
                        {
                            throw InvalidOperandException(std::string("--") + opt_it->second->full_name, argv[ii + 1]);
                        }
                        ++ii; // Next token already parsed as an operand
                    }
                }
                else
                {
                    throw UnknownArgumentException(arg);
                }
            }
            // Single dash, multiple concatenated flags
            else if (!positional_candidate)
            {
                if (char unknown = try_set_flag_group(arg))
                {
                    throw UnknownArgumentException(std::string("-") + unknown);
                }
            }
            // No dash, must be a positional argument
            else
            {
                if (!try_set_positional(current_positional, arg))
                {
                    throw SupernumeraryArgumentException(arg);
                }
            }
        }
    }
    catch (std::exception& e)
    {
        log_error(e.what());
        valid_state_ = false;
    }

    // Check constraints
    valid_state_ &= check_positional_requirements();
    valid_state_ &= check_exclusivity_constraints();
    valid_state_ &= check_dependencies();

    return valid_state_;
}

void ArgParse::set_dependency(char key, char req)
{
    auto opt_it = arguments_.find(key);
    [[maybe_unused]] auto req_it = arguments_.find(req);
    assert(opt_it != arguments_.end() && "Unknown argument");
    assert(req_it != arguments_.end() && "Unknown argument");
    // Check these two do not belong to the same exclusive set
    assert(compatible(key, req) && "Cannot set dependency on mutually exclusive options");
    opt_it->second->dependency = req;
}

bool ArgParse::compatible(char A, char B) const
{
    // If these two arguments have an exclusive set in common, they are not compatible
    const auto* arg_A = arguments_.at(A);
    const auto* arg_B = arguments_.at(B);
    std::set<char> intersection;

    std::set_intersection(arg_A->exclusive_sets.begin(), arg_A->exclusive_sets.end(), arg_B->exclusive_sets.begin(),
                          arg_B->exclusive_sets.end(), std::inserter(intersection, intersection.begin()));
    return (intersection.size() == 0);
}

char ArgParse::try_set_flag_group(const std::string& group) noexcept
{
    for (size_t ii = 1; ii < group.size(); ++ii)
    {
        auto flag_it = arguments_.find(group[ii]);
        if (flag_it != arguments_.end())
        {
            if (flag_it->second->underlying_type() == ArgType::BOOL)
            {
                flag_it->second->is_set = true;
            }
            else
            {
                return group[ii];
            }
        }
        else
        {
            return group[ii];
        }
        // TODO(ndx): Detect if a variable short name was concatenated
        // if so, provide a more precise exception for this specific case
    }
    return 0;
}

bool ArgParse::try_set_positional(size_t& current_positional, const std::string& arg) noexcept(false)
{
    if (current_positional < positionals_.size())
    {
        try
        {
            positionals_[current_positional]->cast(arg);
        }
        catch (std::invalid_argument& e)
        {
            throw InvalidOperandException(std::string("--") + positionals_[current_positional]->full_name, arg);
        }
        positionals_[current_positional]->is_set = true;
        ++current_positional;
        return true;
    }
    return false;
}

bool ArgParse::check_positional_requirements() noexcept
{
    // * Check that all positional arguments are set
    for (auto* parg : positionals_)
    {
        if (!parg->is_set)
        {
            log_error(fmt::format("Missing required argument: {}", parg->full_name));
            return false;
        }
    }

    return true;
}

bool ArgParse::check_exclusivity_constraints() noexcept
{
    // * Check flags exclusivity constraints
    // First, get the set of all active flags
    if (!check_intersection(get_active([](AbstractOption* parg) { return (parg->underlying_type() == ArgType::BOOL); }),
                            exclusive_flags_))
    {
        return false;
    }

    // * Check variables exclusivity constraints
    // First, get the set of all active variables
    if (!check_intersection(get_active([](AbstractOption* parg) { return (parg->underlying_type() != ArgType::BOOL); }),
                            exclusive_variables_))
    {
        return false;
    }

    return true;
}

bool ArgParse::check_dependencies() noexcept
{
    // All dependencies of all arguments in the active set must be in the active set
    auto active_set = get_active([](AbstractOption*) { return true; });
    std::set<char> required, difference;
    for (char key : active_set)
    {
        if (char dep = arguments_.at(key)->dependency)
        {
            required.insert(dep);
        }
    }

    std::set_difference(required.begin(), required.end(), active_set.begin(), active_set.end(),
                        std::inserter(difference, difference.begin()));

    if (difference.size() > 0)
    {
        std::string formatted_message =
            fmt::format("These arguments are required: {}",
                        fmt::join(difference | std::views::transform([this](char key) {
                                      return fmt::format("--{} (-{})", arguments_.at(key)->full_name, key);
                                  }),
                                  ", "));

        log_error(formatted_message);
        return false;
    }

    return true;
}

std::set<char> ArgParse::get_active(std::function<bool(AbstractOption*)> filter) const noexcept
{
    std::set<char> active_set;
    for (auto&& [key, parg] : arguments_)
    {
        if (filter(parg) && parg->is_set)
        {
            active_set.insert(key);
        }
    }

    return active_set;
}

bool ArgParse::check_intersection(const std::set<char> active, const std::vector<std::set<char>>& exclusives) noexcept
{
    // If any intersection of the active set with an exclusive set is of cardinal greater
    // than one, it means the exclusivity constraint was violated
    for (const auto& ex_set : exclusives)
    {
        std::set<char> intersection;

        std::set_intersection(active.begin(), active.end(), ex_set.begin(), ex_set.end(),
                              std::inserter(intersection, intersection.begin()));
        if (intersection.size() > 1)
        {
            std::string formatted_message =
                fmt::format("Incompatible arguments: {}",
                            fmt::join(intersection | std::views::transform([this](char key) {
                                          return fmt::format("--{} (-{})", arguments_.at(key)->full_name, key);
                                      }),
                                      ", "));

            log_error(formatted_message);

            return false;
        }
    }

    return true;
}

void ArgParse::make_usage_string()
{
    // Gather all unconstrained flags & variables
    std::set<char> compat_flags;
    std::set<char> compat_vars;
    std::vector<std::pair<AbstractOption*, AbstractOption*>> args_with_deps;
    std::set<char> blacklist = {'h', 'v'}; // Exclude -h and -v from synopsis
    for (auto&& [key, parg] : arguments_)
    {
        if (parg->dependency != 0)
        {
            args_with_deps.push_back({arguments_.at(key), arguments_.at(parg->dependency)});
            blacklist.insert(parg->dependency);
            continue;
        }
        if (parg->exclusive_sets.size() == 0)
        {
            if (parg->underlying_type() == ArgType::BOOL)
            {
                compat_flags.insert(key);
            }
            else
            {
                compat_vars.insert(key);
            }
        }
    }

    for (char key : blacklist)
    {
        compat_flags.erase(key);
        compat_vars.erase(key);
    }

    std::vector<std::string> parts;

    // Start usage string
    parts.push_back(fmt::format("Usage:\n{}", program_name_));

    // Display nonexclusive flags
    if (!compat_flags.empty())
    {
        parts.push_back(fmt::format("[-{}]", fmt::join(compat_flags, "")));
    }

    // Display exclusive flags
    for (const auto& ex_set : exclusive_flags_)
    {
        parts.push_back(fmt::format(
            "[{}]",
            fmt::join(ex_set | std::views::transform([](char key) { return fmt::format("-{}", key); }), " | ")));
    }

    // Display nonexclusive variables
    for (char key : compat_vars)
    {
        const auto* parg = arguments_.at(key);
        parts.push_back(fmt::format("[-{} <{}>]", parg->short_name, k_type_str.at(parg->underlying_type())));
    }

    // Display exclusive variables
    for (const auto& ex_set : exclusive_variables_)
    {
        parts.push_back(fmt::format("[{}]", fmt::join(ex_set | std::views::transform([this](char key) {
                                                          const auto* parg = arguments_.at(key);
                                                          return fmt::format("-{} <{}>", parg->short_name,
                                                                             k_type_str.at(parg->underlying_type()));
                                                      }),
                                                      " | ")));
    }

    // Display arguments with dependencies
    for (auto&& [parg, preq] : args_with_deps)
    {
        parts.push_back(fmt::format(
            "[-{}{} [-{}{}]]", preq->short_name,
            preq->underlying_type() != ArgType::BOOL ? fmt::format(" <{}>", k_type_str.at(preq->underlying_type()))
                                                     : "",
            parg->short_name,
            parg->underlying_type() != ArgType::BOOL ? fmt::format(" <{}>", k_type_str.at(parg->underlying_type()))
                                                     : ""));
    }

    // Display positional arguments
    for (const auto* parg : positionals_)
    {
        parts.push_back(parg->full_name);
    }

    std::string usage = fmt::format("{}\n", fmt::join(parts, " "));

    // Show argument descriptions
    if (!positionals_.empty())
    {
        usage += "\nWith:\n";
        for (const auto* parg : positionals_)
        {
            usage += parg->format_description(usage_padding_);
        }
    }

    usage += "\nOptions:\n";

    for (auto&& [key, parg] : arguments_)
    {
        if (parg->underlying_type() == ArgType::BOOL)
        {
            usage += parg->format_description(usage_padding_);
        }
    }

    for (auto&& [key, parg] : arguments_)
    {
        if (parg->underlying_type() != ArgType::BOOL)
        {
            usage += parg->format_description(usage_padding_);
        }
    }

    usage_string_.assign(usage);
}

void ArgParse::make_version_string()
{
    std::string pg_upper = program_name_;
    std::transform(pg_upper.begin(), pg_upper.end(), pg_upper.begin(), ::toupper);
    full_ver_string_.assign(fmt::format("{} - version: {}", pg_upper, ver_string_));
}

std::string AbstractOption::format_description(long max_pad) const
{
    std::string option_str;

    if (short_name != 0)
    {
        option_str = fmt::format("-{}, --{}", short_name, full_name);
    }
    else
    {
        option_str = full_name;
    }

    if (underlying_type() != ArgType::BOOL)
    {
        option_str += fmt::format(" <{}>", k_type_str.at(underlying_type()));
    }

    if (dependency)
    {
        option_str += fmt::format(" REQ: -{}", dependency);
    }

    std::string padded_option = fmt::format("    {:<{}}", option_str, max_pad);

    return fmt::format("{}{}\n", padded_option, description);
}

static std::vector<std::string> split(const std::string& input)
{
    std::regex reg(",");
    std::sregex_token_iterator iter(input.begin(), input.end(), reg, -1);
    std::sregex_token_iterator end;
    std::vector<std::string> result(iter, end);
    return result;
}

template <typename T, typename U>
static inline void assign_list(const std::string& operand, std::vector<T>& target, U unary)
{
    auto vstr = split(operand);
    target.resize(vstr.size());
    std::transform(vstr.begin(), vstr.end(), target.begin(), unary);
}

namespace detail
{
template <>
bool StringCast<bool>(const std::string&) noexcept(false)
{
    return true;
}

template <>
int StringCast<int>(const std::string& operand) noexcept(false)
{
    return std::stoi(operand, nullptr, 0);
}

template <>
long StringCast<long>(const std::string& operand) noexcept(false)
{
    return std::stol(operand, nullptr, 0);
}

template <>
float StringCast<float>(const std::string& operand) noexcept(false)
{
    return std::stof(operand);
}

template <>
double StringCast<double>(const std::string& operand) noexcept(false)
{
    return std::stod(operand);
}

template <>
std::string StringCast<std::string>(const std::string& operand) noexcept(false)
{
    return operand;
}

template <>
std::vector<int> StringCast<std::vector<int>>(const std::string& operand) noexcept(false)
{
    std::vector<int> result;
    assign_list(operand, result, [](const std::string& s) -> int { return std::stoi(s, nullptr, 0); });
    return result;
}

template <>
std::vector<long> StringCast<std::vector<long>>(const std::string& operand) noexcept(false)
{
    std::vector<long> result;
    assign_list(operand, result, [](const std::string& s) -> long { return std::stol(s, nullptr, 0); });
    return result;
}

template <>
std::vector<float> StringCast<std::vector<float>>(const std::string& operand) noexcept(false)
{
    std::vector<float> result;
    assign_list(operand, result, [](const std::string& s) -> float { return std::stof(s); });
    return result;
}

template <>
std::vector<double> StringCast<std::vector<double>>(const std::string& operand) noexcept(false)
{
    std::vector<double> result;
    assign_list(operand, result, [](const std::string& s) -> double { return std::stod(s); });
    return result;
}

template <>
std::vector<std::string> StringCast<std::vector<std::string>>(const std::string& operand) noexcept(false)
{
    return split(operand);
}
} // namespace detail
} // namespace ap
} // namespace kb