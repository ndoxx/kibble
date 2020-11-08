#include "argparse/argparse.h"
#include "logger/logger.h"

#include <algorithm>
#include <cassert>
#include <exception>

namespace kb
{
namespace ap
{

struct IllegalSyntaxException : public std::exception
{
    const char* what() const noexcept override { return "Exception: illegal syntax"; }
};

struct UnknownArgumentException : public std::exception
{
    UnknownArgumentException(const std::string& argument) : argument_(argument) {}
    const char* what() const noexcept override { return "Exception: unknown argument"; }
    std::string argument_;
};

struct SupernumeraryArgumentException : public std::exception
{
    SupernumeraryArgumentException(const std::string& argument) : argument_(argument) {}
    const char* what() const noexcept override { return "Exception: supernumerary argument"; }
    std::string argument_;
};

struct MissingValueException : public std::exception
{
    MissingValueException(const std::string& variable) : variable_(variable) {}
    const char* what() const noexcept override { return "Exception: missing value after argument"; }
    std::string variable_;
};

ArgParse::ArgParse(const std::string& program_name, const std::string& ver_string)
    : ver_string_(ver_string), program_name_(program_name), valid_state_(false), was_run_(false), argc_(0)
{}

ArgParse::~ArgParse()
{
    for(auto&& [key, pvar] : variables_)
        delete pvar;
    for(auto* pvar : positionals_)
        delete pvar;
}

void ArgParse::add_flag(char short_name, const std::string& full_name, const std::string& description,
                        bool default_value)
{
    [[maybe_unused]] auto findit = flags_.find(short_name);
    assert(findit == flags_.end() && "Flag argument already existing at this name.");

    flags_.insert(std::pair(short_name, ArgFlag{default_value, short_name, full_name, description}));
    full_to_short_.insert(std::pair(full_name, short_name));
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
    argc_ = argc;
    // program_name_ = argv[0];
    valid_state_ = true;
    was_run_ = true;
    size_t current_positional = 0;

    try
    {
        // For all tokens in argv
        for(int ii = 1; ii < argc; ++ii)
        {
            std::string arg = argv[ii];

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
                    throw UnknownArgumentException(arg.substr(2));
            }
            if(single_short)
                key = arg[1];

            // Single argument
            if(single_short || single_full)
            {
                if(try_set_flag(key))
                    continue; // Token parsed correctly as a flag

                // Current token is the last one, so operand is missing
                if(ii == argc - 1)
                    throw MissingValueException(arg);

                if(try_set_variable(key, argv[ii + 1]))
                {
                    ++ii;     // Next token already parsed as an operand
                    continue; // Current token parsed correctly as a variable
                }

                throw UnknownArgumentException(std::string(1, arg[size_t(ii)]));
            }
            // Single dash, multiple concatenated flags
            else if(!positional_candidate)
            {
                if(char unknown = try_set_flag_group(arg))
                    throw UnknownArgumentException(std::string(1, unknown));
            }
            // No dash, must be a positional argument
            else
            {
                if(!try_set_positional(current_positional, arg))
                    throw SupernumeraryArgumentException(arg);
            }
        }
    }
    catch(UnknownArgumentException& e)
    {
        KLOGE("kibble") << "Unknown argument: " << e.argument_ << std::endl;
        valid_state_ = false;
    }
    catch(MissingValueException& e)
    {
        KLOGE("kibble") << "Missing value for argument: " << e.variable_ << std::endl;
        valid_state_ = false;
    }
    catch(std::exception& e)
    {
        KLOGE("kibble") << e.what() << std::endl;
        valid_state_ = false;
    }

    valid_state_ &= check_requirements();

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

bool ArgParse::check_requirements() noexcept
{
    // Check that all positional arguments are set
    for(auto* pvar : positionals_)
    {
        if(!pvar->is_set)
        {
            KLOGE("kibble") << "Missing required argument: " << pvar->full_name << std::endl;
            return false;
        }
    }

    return true;
}

void ArgParse::usage() const
{
    std::string flags = "";
    for(auto&& [key, flag] : flags_)
        flags += key;

    KLOG("kibble", 1) << "Usage:" << std::endl;
    KLOG("kibble", 1) << program_name_;
    if(flags.size())
    {
        KLOG("kibble", 1) << " [-" << flags << ']';
    }
    for(auto&& [key, var] : variables_)
    {
        KLOG("kibble", 1) << " [-" << var->short_name << " <" << var->underlying_type() << ">]";
    }
    for(const auto* var : positionals_)
    {
        KLOG("kibble", 1) << ' ' << var->full_name;
    }
    KLOG("kibble", 1) << std::endl;

    KLOG("kibble", 1) << std::endl << "With:" << std::endl;
    KLOG("kibble", 1) << "--help" << std::endl;
    KLOGI << "Display this usage string and exit." << std::endl;
    for(const auto* var : positionals_)
    {
        KLOG("kibble", 1) << var->full_name << " <" << var->underlying_type() << ">:" << std::endl;
        KLOGI << var->description << std::endl;
    }
    for(auto&& [key, flag] : flags_)
    {
        KLOG("kibble", 1) << '-' << flag.short_name << " (--" << flag.full_name << "):" << std::endl;
        KLOGI << flag.description << std::endl;
    }
    for(auto&& [key, var] : variables_)
    {
        KLOG("kibble", 1) << '-' << var->short_name << " (--" << var->full_name << ") " << var->underlying_type() << ':'
                          << std::endl;
        KLOGI << var->description << std::endl;
    }
}

void ArgParse::version() const
{
    std::string pg_upper = program_name_;
    std::transform(pg_upper.begin(), pg_upper.end(), pg_upper.begin(), ::toupper);
    KLOG("kibble", 1) << pg_upper << " - version: " << ver_string_ << std::endl;
}

void ArgParse::debug_report() const
{
    if(!was_run_)
    {
        KLOGW("kibble") << "Parser was not run." << std::endl;
        return;
    }

    if(!valid_state_)
    {
        KLOGE("kibble") << "Invalid command line, could not parse arguments." << std::endl;
        return;
    }

    for(auto&& [key, flag] : flags_)
    {
        if(flag.value)
        {
            KLOG("kibble", 1) << "[FLAG] " << flag.full_name << " (" << flag.short_name << ')' << std::endl;
        }
    }

    for(auto&& [key, var] : variables_)
    {
        if(var->is_set)
        {
            // TODO: value as a string
            KLOG("kibble", 1) << "[VAR]  " << var->full_name << " (" << var->short_name << ") = " << std::endl;
        }
    }
}

} // namespace ap
} // namespace kb