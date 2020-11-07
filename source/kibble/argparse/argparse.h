#pragma once

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <cassert>
#include <exception>
#include <memory>

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

struct UnknownFlagException: public std::exception
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
	ArgParse(const std::string& description, const std::string& ver_string);

	inline const std::string& get_description() const { return description_; }
	inline const std::string& get_version_string() const { return ver_string_; }

	inline bool is_set(char short_name) const
	{
		assert(success_ && "Parser wasn't run or parsing error.");
		auto findit = flags_.find(short_name);
		assert(findit != flags_.end() && "Unknown flag argument.");
		return findit->second.value;
	}

	void add_flag(char short_name, const std::string& full_name, const std::string& description, bool default_value = false);
	bool parse(int argc, char** argv);
	
	void debug_report() const;

private:
	std::string get_flags(const std::string& arg) const;

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