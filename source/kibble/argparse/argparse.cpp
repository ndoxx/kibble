#include "argparse/argparse.h"
#include "logger/logger.h"

#include <exception>
#include <cassert>

namespace kb
{
namespace ap
{

struct IllegalSyntaxException: public std::exception
{
	const char* what() const noexcept override { return "Exception: illegal syntax"; }
};

struct UnknownArgumentException: public std::exception
{
	UnknownArgumentException(const std::string& argument): argument_(argument) {}
	const char* what() const noexcept override { return "Exception: unknown argument"; }
	std::string argument_;
};

struct MissingValueException: public std::exception
{
	MissingValueException(const std::string& variable): variable_(variable) {}
	const char* what() const noexcept override { return "Exception: missing value after argument"; }
	std::string variable_;
};

ArgParse::ArgParse(const std::string& description, const std::string& ver_string):
description_(description),
ver_string_(ver_string),
valid_state_(false),
was_run_(false),
argc_(0)
{

}

ArgParse::~ArgParse()
{
	for(auto&& [key, pvar]: variables_)
		delete pvar;
}

void ArgParse::add_flag(char short_name, const std::string& full_name, const std::string& description, bool default_value)
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
	program_name_ = argv[0];
	valid_state_ = true;
	was_run_ = true;

	try
	{
		// For all tokens in argv
		for(int ii = 1; ii < argc; ++ii)
		{
			std::string arg = argv[ii];

			bool single_short = (arg.size() == 2 && arg[0] == '-');
			bool single_full = (arg[0] == '-' && arg[1] == '-');
			char key = 0;

			if(single_full)
			{
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
				auto flag_it = flags_.find(key);
				if(flag_it != flags_.end())
				{
					flag_it->second.value = true;
					continue;
				}
				auto var_it = variables_.find(key);
				if(var_it != variables_.end())
				{
					auto* pvar = var_it->second;
					if(ii == argc-1)
						throw MissingValueException(pvar->full_name);

					pvar->string_value = argv[ii+1];
					pvar->cast(argv[ii+1]);
					pvar->is_set = true;
					++ii;
					continue;
				}

				throw UnknownArgumentException(std::string(1, arg[size_t(ii)]));
			}
			// Single dash, multiple concatenated flags
			else
			{
				for(size_t jj = 1; jj < arg.size(); ++jj)
				{
					if(flags_.find(arg[jj]) == flags_.end())
						throw UnknownArgumentException(std::string(1, arg[jj]));
					// TODO(ndx): Detect if a variable short name was concatenated
					// if so, provide a more precise exception for this specific case
				}
				for(size_t jj = 1; jj < arg.size(); ++jj)
					flags_.at(arg[jj]).value = true;
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

	return valid_state_;
}

void ArgParse::usage() const
{
	// TODO(ndx): Display help message, given the submitted model
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

	for(auto&& [key,flag]: flags_)
	{
		if(flag.value)
		{
			KLOG("kibble",1) << "[FLAG] " << flag.full_name << " (" << flag.short_name << ')' << std::endl;
		}
	}

	for(auto&& [key,var]: variables_)
	{
		if(var->is_set)
		{
			KLOG("kibble",1) << "[VAR]  " << var->full_name << " (" << var->short_name << ") = " << var->string_value << std::endl;
		}
	}
}

} // namespace kb
} // namespace ap