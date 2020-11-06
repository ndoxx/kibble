#pragma once

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <cassert>
#include <exception>
#include "logger/logger.h"

namespace kb
{
namespace ap
{

struct IllegalSyntaxException: public std::exception
{
	const char* what () const throw ()
	{
		return "Exception: illegal syntax";
	}
};

struct UnknownArgumentException: public std::exception
{
	const char* what () const throw ()
	{
		return "Exception: unknown argument";
	}
};

struct ArgFlag
{
	bool value;
	char short_name;
	std::string full_name;
	std::string description;
};

class ArgParse
{
public:
	ArgParse(const std::string& description, const std::string& ver_string):
	description_(description),
	ver_string_(ver_string),
	success_(false),
	argc_(0)
	{

	}

	inline const std::string& get_description() const { return description_; }
	inline const std::string& get_version_string() const { return ver_string_; }

	void add_flag(char short_name, const std::string& full_name, const std::string& description, bool default_value = false)
	{
		[[maybe_unused]] auto findit = flags_.find(short_name);
		assert(findit == flags_.end() && "Flag argument already existing at this name.");

		flags_.insert(std::pair(short_name, ArgFlag{default_value, short_name, full_name, description}));
		full_to_short_.insert(std::pair(full_name, short_name));
	}

	inline bool is_set(char short_name) const
	{
		assert(success_ && "Parser wasn't run or parsing error.");
		auto findit = flags_.find(short_name);
		assert(findit != flags_.end() && "Unknown flag argument.");
		return findit->second.value;
	}

	bool parse(int argc, char** argv)
	{
		assert(argc > 0 && "Arg count should be a strictly positive integer.");
		argc_ = argc;
		program_name_ = argv[0];
		success_ = true;

		try
		{
			for(int ii = 1; ii < argc; ++ii)
			{
				char key = 0;
				if(get_key(argv[ii], key))
				{
					// Is it a flag?
					auto flag_it = flags_.find(key);
					if(flag_it != flags_.end())
					{
						flag_it->second.value = true;
						continue;
					}
				}
			}
		}
		catch(std::exception& e)
		{
			KLOGE("kibble") << e.what() << std::endl;
			success_ = false;
		}

		// NOTE(ndx): also check no unknown argument was given
		return success_;
	}

private:
	bool get_key(const std::string& arg, char& key) const
	{
		// Not a flag and not a variable
		if(arg[0] != '-')
			return false;

		if(arg.size() == 2)
		{
			key = arg[1];
			return true;
		}
		else
		{
			if(arg[1] != '-')
				throw IllegalSyntaxException();

			auto full_it = full_to_short_.find(arg.substr(2));
			if(full_it != full_to_short_.end())
			{
				key = full_it->second;
				return true;
			}
			else
				throw UnknownArgumentException();
		}
	}

private:
	std::string description_;
	std::string ver_string_;
	std::string program_name_;
	std::map<char, ArgFlag> flags_;
	std::unordered_map<std::string, char> full_to_short_;
	bool success_;

	int argc_;
};

} // namespace kb
} // namespace ap