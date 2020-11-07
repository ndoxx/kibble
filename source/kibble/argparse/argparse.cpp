#include "argparse/argparse.h"
#include "logger/logger.h"

namespace kb
{
namespace ap
{

ArgParse::ArgParse(const std::string& description, const std::string& ver_string):
description_(description),
ver_string_(ver_string),
success_(false),
argc_(0)
{

}

void ArgParse::add_flag(char short_name, const std::string& full_name, const std::string& description, bool default_value)
{
	[[maybe_unused]] auto findit = flags_.find(short_name);
	assert(findit == flags_.end() && "Flag argument already existing at this name.");

	flags_.insert(std::pair(short_name, ArgFlag{default_value, short_name, full_name, description}));
	full_to_short_.insert(std::pair(full_name, short_name));
}

bool ArgParse::parse(int argc, char** argv)
{
	assert(argc > 0 && "Arg count should be a strictly positive integer.");
	argc_ = argc;
	program_name_ = argv[0];
	success_ = true;

	try
	{
		for(int ii = 1; ii < argc; ++ii)
		{
			auto input_flags = get_flags(argv[ii]);
			for(char f : input_flags)
				flags_.at(f).value = true;
			if(input_flags.size())
				continue;
		}
	}
	catch(std::exception& e)
	{
		KLOGE("kibble") << e.what() << std::endl;
		success_ = false;
	}

	return success_;
}

void ArgParse::debug_report() const
{
	for(auto&& [key,flag]: flags_)
	{
		if(flag.value)
		{
			KLOG("kibble",1) << "[FLAG] " << flag.full_name << " (" << flag.short_name << ')' << std::endl;
		}
	}
}

std::string ArgParse::get_flags(const std::string& arg) const
{
	// Not a flag and not a variable
	if(arg[0] != '-')
		return "";

	// Full name was used
	if(arg[1] == '-')
	{
		auto full_it = full_to_short_.find(arg.substr(2));
		if(full_it != full_to_short_.end())
			return std::string(1, full_it->second);
		else
			throw UnknownFlagException();
	}
	// Short name is used, but maybe there are several options concatenated
	// all of whitch MUST be flags
	else
	{
		for(size_t ii = 1; ii < arg.size(); ++ii)
		{
			auto findit = flags_.find(arg[ii]);
			if(findit == flags_.end())
				throw UnknownFlagException();
		}
		return arg.substr(1);
	}

	return "";
}

} // namespace kb
} // namespace ap