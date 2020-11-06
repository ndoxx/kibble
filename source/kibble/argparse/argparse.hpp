#pragma once

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <cassert>
// #include "logger/logger.h"

namespace kb
{
namespace ap
{

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
		flag_full_to_short_.insert(std::pair(full_name, short_name));
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
		bool all_legal = true;

		for(int ii = 1; ii < argc; ++ii)
		{
			all_legal &= parse_flag(argv[ii]);
		}

		// NOTE(ndx): also check no unknown argument was given
		success_ = all_legal;
		return success_;
	}

private:
	bool parse_flag(const std::string& arg)
	{
		// Not a flag
		if(arg[0] != '-')
			return true;

		if(arg.size() == 2)
		{
			char key = arg[1];
			auto findit = flags_.find(key);
			if(findit != flags_.end())
				findit->second.value = true;
		}
		else
		{
			// Long names MUST use a double-dash
			if(arg[1] != '-')
				return false;

			auto findit = flag_full_to_short_.find(arg.substr(2));
			if(findit != flag_full_to_short_.end())
				flags_.at(findit->second).value = true;
		}

		return true;
	}

private:
	std::string description_;
	std::string ver_string_;
	std::string program_name_;
	std::map<char, ArgFlag> flags_;
	std::unordered_map<std::string, char> flag_full_to_short_;
	bool success_;

	int argc_;
};

} // namespace kb
} // namespace ap