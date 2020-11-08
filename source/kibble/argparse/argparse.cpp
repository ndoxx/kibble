#include "argparse/argparse.h"
#include "logger/logger.h"

#include <algorithm>
#include <cassert>
#include <exception>

namespace kb
{
namespace ap
{

// Error logging is implemented as a side-effect of what(). Uggly, but it works.
struct IllegalSyntaxException : public std::exception
{
    const char* what() const noexcept override
    {
        KLOGW("core") << "Illegal syntax." << std::endl;
        return "Exception: illegal syntax";
    }
};

struct UnknownArgumentException : public std::exception
{
    UnknownArgumentException(const std::string& argument) : argument_(argument) {}
    const char* what() const noexcept override
    {
        KLOGW("core") << "Unknown argument: " << argument_ << std::endl;
        return "Exception: unknown argument";
    }
    std::string argument_;
};

struct SupernumeraryArgumentException : public std::exception
{
    SupernumeraryArgumentException(const std::string& argument) : argument_(argument) {}
    const char* what() const noexcept override
    {
        KLOGW("core") << "Supernumerary argument: " << argument_ << std::endl;
        return "Exception: supernumerary argument";
    }
    std::string argument_;
};

struct MissingOperandException : public std::exception
{
    MissingOperandException(const std::string& argument) : argument_(argument) {}
    const char* what() const noexcept override
    {
        KLOGW("core") << "Missing operand after argument: " << argument_ << std::endl;
        return "Exception: missing operand after argument";
    }
    std::string argument_;
};

struct InvalidOperandException : public std::exception
{
    InvalidOperandException(const std::string& argument, const std::string& value) : argument_(argument), value_(value)
    {}
    const char* what() const noexcept override
    {
        KLOGW("core") << "Invalid operand: \"" << value_ << "\" for argument: " << argument_ << std::endl;
        return "Exception: invalid operand";
    }
    std::string argument_;
    std::string value_;
};

ArgParse::ArgParse(const std::string& program_name, const std::string& ver_string)
    : ver_string_(ver_string), program_name_(program_name), valid_state_(false), was_run_(false)
{}

ArgParse::~ArgParse()
{
    for(auto&& [key, pvar] : variables_)
        delete pvar;
    for(auto* pvar : positionals_)
        delete pvar;
}

const ArgFlag& ArgParse::add_flag(char short_name, const std::string& full_name, const std::string& description,
                                  bool default_value)
{
    [[maybe_unused]] auto findit = flags_.find(short_name);
    assert(findit == flags_.end() && "Flag argument already existing at this name.");

    flags_.insert(std::pair(short_name, ArgFlag{0, default_value, short_name, full_name, description}));
    full_to_short_.insert(std::pair(full_name, short_name));

    return flags_.at(short_name);
}

void ArgParse::set_flags_exclusive(const std::set<char>& exclusive_set)
{
    for(char key : exclusive_set)
    {
        auto flag_it = flags_.find(key);
        assert(flag_it != flags_.end() && "Unknown flag.");
        flag_it->second.exclusive_idx = exclusive_flags_.size() + 1;
    }

    exclusive_flags_.push_back(exclusive_set);
}

void ArgParse::set_variables_exclusive(const std::set<char>& exclusive_set)
{
    for(char key : exclusive_set)
    {
        auto var_it = variables_.find(key);
        assert(var_it != variables_.end() && "Unknown variable.");
        var_it->second->exclusive_idx = exclusive_variables_.size() + 1;
    }

    exclusive_variables_.push_back(exclusive_set);
}

bool ArgParse::is_set(char short_name) const
{
    assert(was_run_ && "Parser was not run.");
    assert(valid_state_ && "Invalid command line.");
    auto findit = flags_.find(short_name);
    assert(findit != flags_.end() && "Unknown flag argument.");
    return findit->second.value;
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
        for(int ii = 1; ii < argc; ++ii)
        {
            std::string arg = argv[ii];

            // Detect dash / double dash syntax
            bool single_short = (arg.size() == 2 && arg[0] == '-');
            bool single_full = (arg[0] == '-' && arg[1] == '-');
            bool positional_candidate = (arg[0] != '-');
            char key = 0;

            if(single_full)
            {
                if(try_match_special_options(arg))
                    exit(0);

                auto full_it = full_to_short_.find(arg.substr(2));
                if(full_it != full_to_short_.end())
                    key = full_it->second;
                else
                    throw UnknownArgumentException(arg);
            }
            if(single_short)
                key = arg[1];

            // Single argument
            if(single_short || single_full)
            {
                if(try_set_flag(key))
                    continue; // Token parsed correctly as a flag

                // If current token is the last one, operand is missing
                if(ii == argc - 1)
                    throw MissingOperandException(arg);

                try
                {
                    if(try_set_variable(key, argv[ii + 1]))
                    {
                        ++ii;     // Next token already parsed as an operand
                        continue; // Current token parsed correctly as a variable
                    }
                }
                catch(std::invalid_argument& e)
                {
                    // Got here because stoi/l/f/d did not manage to parse the operand
                    throw InvalidOperandException(arg, argv[ii + 1]);
                }

                throw UnknownArgumentException(arg);
            }
            // Single dash, multiple concatenated flags
            else if(!positional_candidate)
            {
                if(char unknown = try_set_flag_group(arg))
                    throw UnknownArgumentException(std::string("-") + unknown);
            }
            // No dash, must be a positional argument
            else
            {
                if(!try_set_positional(current_positional, arg))
                    throw SupernumeraryArgumentException(arg);
            }
        }
    }
    catch(std::exception& e)
    {
        // Error logging is implemented as a side-effect of what(). Uggly, but it works.
        e.what();
        valid_state_ = false;
    }

    // Check constraints
    valid_state_ &= check_positional_requirements();
    valid_state_ &= check_exclusivity_constraints();

    return valid_state_;
}

bool ArgParse::try_match_special_options(const std::string& arg) noexcept
{
    // --help => show usage and exit
    if(!arg.compare("--help"))
    {
        usage();
        return true;
    }

    // --version => show version string and exit
    if(!arg.compare("--version"))
    {
        version();
        return true;
    }
    return false;
}

bool ArgParse::try_set_flag(char key) noexcept
{
    auto flag_it = flags_.find(key);
    if(flag_it != flags_.end())
    {
        flag_it->second.value = true;
        return true;
    }
    return false;
}

char ArgParse::try_set_flag_group(const std::string& group) noexcept
{
    for(size_t ii = 1; ii < group.size(); ++ii)
    {
        if(flags_.find(group[ii]) != flags_.end())
            flags_.at(group[ii]).value = true;
        else
            return group[ii];
        // TODO(ndx): Detect if a variable short name was concatenated
        // if so, provide a more precise exception for this specific case
    }
    return 0;
}

bool ArgParse::try_set_variable(char key, const std::string& operand) noexcept(false)
{
    auto var_it = variables_.find(key);
    if(var_it != variables_.end())
    {
        auto* pvar = var_it->second;
        pvar->cast(operand);
        pvar->is_set = true;
        return true;
    }
    return false;
}

bool ArgParse::try_set_positional(size_t& current_positional, const std::string& arg) noexcept(false)
{
    if(current_positional < positionals_.size())
    {
        positionals_[current_positional]->cast(arg);
        positionals_[current_positional]->is_set = true;
        ++current_positional;
        return true;
    }
    return false;
}

bool ArgParse::check_positional_requirements() const noexcept
{
    // * Check that all positional arguments are set
    for(auto* pvar : positionals_)
    {
        if(!pvar->is_set)
        {
            KLOGW("core") << "Missing required argument: --" << pvar->full_name << std::endl;
            return false;
        }
    }

    return true;
}

bool ArgParse::check_exclusivity_constraints() const noexcept
{
    // * Check flags exclusivity constraints
    {
        // First, get the set of all active flags
        auto active_set = get_active_flags();

        if(!check_intersection(active_set, exclusive_flags_, [this](char key) {
               KLOGW("core") << "--" << flags_.at(key).full_name << " (-" << key << ')';
           }))
            return false;
    }

    // * Check variables exclusivity constraints
    {
        // First, get the set of all active variables
        auto active_set = get_active_variables();

        if(!check_intersection(active_set, exclusive_variables_, [this](char key) {
               KLOGW("core") << "--" << variables_.at(key)->full_name << " (-" << key << ')';
           }))
            return false;
    }

    return true;
}

std::set<char> ArgParse::get_active_flags() const noexcept
{
    std::set<char> active_set;
    for(auto&& [key, flag] : flags_)
        if(flag.value)
            active_set.insert(key);

    return active_set;
}

std::set<char> ArgParse::get_active_variables() const noexcept
{
    std::set<char> active_set;
    for(auto&& [key, pvar] : variables_)
        if(pvar->is_set)
            active_set.insert(key);

    return active_set;
}

bool ArgParse::check_intersection(const std::set<char> active, const std::vector<std::set<char>>& exclusives,
                                  std::function<void(char)> show_argument) const noexcept
{
    // If any intersection of the active set with an exclusive set is of cardinal greater
    // than one, it means the exclusivity constraint was violated
    for(const auto& ex_set : exclusives)
    {
        std::set<char> intersection;

        std::set_intersection(active.begin(), active.end(), ex_set.begin(), ex_set.end(),
                              std::inserter(intersection, intersection.begin()));
        if(intersection.size() > 1)
        {
            KLOGW("core") << "Incompatible arguments: ";
            size_t count = 0;
            for(char key : intersection)
            {
                show_argument(key);
                if(++count < intersection.size())
                {
                    KLOGW("core") << ", ";
                }
            }
            KLOGW("core") << std::endl;
            return false;
        }
    }

    return true;
}

void ArgParse::usage() const
{
    // Gather all non exclusive flags
    std::string nonex_flags = "";
    for(auto&& [key, flag] : flags_)
        if(flag.exclusive_idx == 0)
            nonex_flags += key;

    // Gather all non exclusive variables
    std::string nonex_vars = "";
    for(auto&& [key, pvar] : variables_)
        if(pvar->exclusive_idx == 0)
            nonex_vars += key;

    // Start usage string
    KLOG("core", 1) << "Usage:" << std::endl;
    KLOG("core", 1) << program_name_;

    // Display nonexclusive flags
    if(nonex_flags.size())
    {
        KLOG("core", 1) << " [-" << nonex_flags << ']';
    }

    // Display exclusive flags
    for(const auto& ex_set : exclusive_flags_)
    {
        size_t count = 0;
        KLOG("core", 1) << " [";
        for(char key : ex_set)
        {
            KLOG("core", 1) << '-' << key;
            if(++count < ex_set.size())
            {
                KLOG("core", 1) << " | ";
            }
        }
        KLOG("core", 1) << ']';
    }

    // Display nonexclusive variables
    for(char key : nonex_vars)
    {
        const auto* pvar = variables_.at(key);
        KLOG("core", 1) << " [-" << pvar->short_name << " <" << pvar->underlying_type() << ">]";
    }

    // Display exclusive variables
    for(const auto& ex_set : exclusive_variables_)
    {
        size_t count = 0;
        KLOG("core", 1) << " [";
        for(char key : ex_set)
        {
            const auto* pvar = variables_.at(key);
            KLOG("core", 1) << '-' << pvar->short_name << " <" << pvar->underlying_type() << '>';
            if(++count < ex_set.size())
            {
                KLOG("core", 1) << " | ";
            }
        }
        KLOG("core", 1) << ']';
    }

    // Display positional arguments
    for(const auto* var : positionals_)
    {
        KLOG("core", 1) << ' ' << var->full_name;
    }
    KLOG("core", 1) << std::endl;

    // Show argument descriptions
    KLOG("core", 1) << std::endl << "With:" << std::endl;

    for(const auto* var : positionals_)
    {
        KLOG("core", 1) << var->full_name << " <" << var->underlying_type() << ">: " << var->description << std::endl;
    }

    KLOG("core", 1) << "--help\tDisplay this usage string and exit." << std::endl;
    KLOG("core", 1) << "--version\tDisplay program version string and exit." << std::endl;
    for(auto&& [key, flag] : flags_)
    {
        KLOG("core", 1) << '-' << flag.short_name << " (--" << flag.full_name << "): " << flag.description << std::endl;
    }

    for(auto&& [key, var] : variables_)
    {
        KLOG("core", 1) << '-' << var->short_name << " (--" << var->full_name << ") " << var->underlying_type() << ": "
                        << var->description << std::endl;
    }
}

void ArgParse::version() const
{
    std::string pg_upper = program_name_;
    std::transform(pg_upper.begin(), pg_upper.end(), pg_upper.begin(), ::toupper);
    KLOG("core", 1) << pg_upper << " - version: " << ver_string_ << std::endl;
}

} // namespace ap
} // namespace kb