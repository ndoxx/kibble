#include "argparse/argparse.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <sstream>

namespace kb
{
namespace ap
{

struct ParsingException: public std::exception
{
    std::string message_;
    const char* what() const noexcept override { return message_.c_str(); }
};

struct UnknownArgumentException: public ParsingException
{
    UnknownArgumentException(const std::string& argument)
    {
        std::stringstream ss;
        ss << "Unknown argument: " << argument;
        message_.assign(ss.str());
    }
};

struct SupernumeraryArgumentException : public ParsingException
{
    SupernumeraryArgumentException(const std::string& argument)
    {
        std::stringstream ss;
        ss << "Supernumerary argument: " << argument;
        message_.assign(ss.str());
    }
};

struct MissingOperandException : public ParsingException
{
    MissingOperandException(const std::string& argument)
    {
        std::stringstream ss;
        ss << "Missing operand after argument: " << argument;
        message_.assign(ss.str());
    }
};

struct InvalidOperandException : public ParsingException
{
    InvalidOperandException(const std::string& argument, const std::string& value)
    {
        std::stringstream ss;
        ss << "Invalid operand: \"" << value << "\" for argument: " << argument;
        message_.assign(ss.str());
    }
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
        log_error(e.what());
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
        output_(usage());
        return true;
    }

    // --version => show version string and exit
    if(!arg.compare("--version"))
    {
        output_(version());
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

bool ArgParse::check_positional_requirements() noexcept
{
    // * Check that all positional arguments are set
    for(auto* pvar : positionals_)
    {
        if(!pvar->is_set)
        {
            std::stringstream ss;
            ss << "Missing required argument: " << pvar->full_name << std::endl;
            log_error(ss.str());
            return false;
        }
    }

    return true;
}

bool ArgParse::check_exclusivity_constraints() noexcept
{
    // * Check flags exclusivity constraints
    {
        // First, get the set of all active flags
        auto active_set = get_active_flags();

        if(!check_intersection(active_set, exclusive_flags_, [this](std::stringstream& ss, char key) {
               ss << "--" << flags_.at(key).full_name << " (-" << key << ')';
           }))
            return false;
    }

    // * Check variables exclusivity constraints
    {
        // First, get the set of all active variables
        auto active_set = get_active_variables();

        if(!check_intersection(active_set, exclusive_variables_, [this](std::stringstream& ss, char key) {
               ss << "--" << variables_.at(key)->full_name << " (-" << key << ')';
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
                                  std::function<void(std::stringstream&, char)> show_argument) noexcept
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
            std::stringstream ss;
            ss << "Incompatible arguments: ";
            size_t count = 0;
            for(char key : intersection)
            {
                show_argument(ss, key);
                if(++count < intersection.size())
                {
                    ss << ", ";
                }
            }
            ss << std::endl;
            log_error(ss.str());
            return false;
        }
    }

    return true;
}

static void format_description(std::stringstream& oss, char key, const std::string& full_name, const std::string& type,
                               const std::string& description, long max_left)
{
    std::stringstream ss;

    ss << "    ";
    if(key != 0)
        ss << '-' << key << ", ";
    if(type.size()==0 || key != 0)
        ss << "--";
    ss << full_name;
    if(type.size())
        ss << " <" << type << '>';

    ss.seekg(0, std::ios::end);
    long size = ss.tellg();
    ss.seekg(0, std::ios::beg);

    long padding = std::max(max_left - size, 0l);

    while(padding>=0)
    {
        ss << ' ';
        --padding;
    }

    oss << ss.str() << description << std::endl;
}

void ArgParse::make_usage_string()
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

    std::stringstream ss;

    // Start usage string
    ss << "Usage:" << std::endl;
    ss << program_name_;

    // Display nonexclusive flags
    if(nonex_flags.size())
    {
        ss << " [-" << nonex_flags << ']';
    }

    // Display exclusive flags
    for(const auto& ex_set : exclusive_flags_)
    {
        size_t count = 0;
        ss << " [";
        for(char key : ex_set)
        {
            ss << '-' << key;
            if(++count < ex_set.size())
            {
                ss << " | ";
            }
        }
        ss << ']';
    }

    // Display nonexclusive variables
    for(char key : nonex_vars)
    {
        const auto* pvar = variables_.at(key);
        ss << " [-" << pvar->short_name << " <" << pvar->underlying_type() << ">]";
    }

    // Display exclusive variables
    for(const auto& ex_set : exclusive_variables_)
    {
        size_t count = 0;
        ss << " [";
        for(char key : ex_set)
        {
            const auto* pvar = variables_.at(key);
            ss << '-' << pvar->short_name << " <" << pvar->underlying_type() << '>';
            if(++count < ex_set.size())
            {
                ss << " | ";
            }
        }
        ss << ']';
    }

    // Display positional arguments
    for(const auto* pvar : positionals_)
    {
        ss << ' ' << pvar->full_name;
    }
    ss << std::endl;

    // Show argument descriptions
    ss << std::endl << "With:" << std::endl;

    for(const auto* pvar : positionals_)
        format_description(ss, pvar->short_name, pvar->full_name, pvar->underlying_type(), pvar->description, usage_padding_);

    ss << std::endl << "Options:" << std::endl;

    format_description(ss, 0, "help", "", "Display this usage string and exit", usage_padding_);
    format_description(ss, 0, "version", "", "Display the program version string and exit", usage_padding_);

    for(auto&& [key, flag] : flags_)
        format_description(ss, flag.short_name, flag.full_name, "", flag.description, usage_padding_);

    for(auto&& [key, pvar] : variables_)
        format_description(ss, pvar->short_name, pvar->full_name, pvar->underlying_type(), pvar->description, usage_padding_);

    usage_string_.assign(ss.str());
}

void ArgParse::make_version_string()
{
    std::stringstream ss;
    std::string pg_upper = program_name_;
    std::transform(pg_upper.begin(), pg_upper.end(), pg_upper.begin(), ::toupper);
    ss << pg_upper << " - version: " << ver_string_ << std::endl;
    full_ver_string_.assign(ss.str());
}

} // namespace ap
} // namespace kb