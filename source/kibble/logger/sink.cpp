#include "logger/sink.h"
#include "logger/common.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string_view>

namespace kb
{
namespace klog
{

#if ANSI_3
static constexpr std::string_view k_code_file_style{"\033[1;39m"};
static constexpr std::string_view k_code_line_style{"\033[1;31m"};
static constexpr std::string_view k_timestamp_style{"\033[1;32m"};
static constexpr std::string_view k_log_files_style{"\033[1;32m"};
#else
static constexpr std::string_view k_code_file_style{"\033[1;38;2;255;255;255m"};
static constexpr std::string_view k_code_line_style{"\033[1;38;2;255;90;90m"};
static constexpr std::string_view k_timestamp_style{"\033[1;38;2;0;130;10m"};
static constexpr std::string_view k_log_files_style{"\033[1;38;2;90;255;90m"};
#endif

void ConsoleSink::send(const LogStatement& stmt, const LogChannel& chan)
{
    if(stmt.msg_type != MsgType::RAW)
    {
        // Show file and line if sufficiently severe
        if(stmt.severity >= 2)
        {
            std::cout << k_code_file_style << "@ " << stmt.code_file << ":" << k_code_line_style << stmt.code_line
                      << "\n";
        }

        float ts = std::chrono::duration_cast<std::chrono::duration<float>>(stmt.timestamp).count();
        std::cout << k_timestamp_style << "[" << std::setprecision(6) << std::fixed << ts << "]"
                  << "\033[0m";
        std::cout << chan.tag << " " << Style::s_colors[size_t(stmt.msg_type)] << Style::s_icons[size_t(stmt.msg_type)]
                  << stmt.message;
    }
    else
        std::cout << "\033[0m" << stmt.message << "\033[0m";
}

void ConsoleSink::send_raw(const std::string& message) { std::cout << message; }

LogFileSink::LogFileSink(const std::string& filename) : filename_(filename) {}

void LogFileSink::send(const LogStatement& stmt, const LogChannel&)
{
    if(stmt.msg_type != MsgType::RAW)
    {
        // Show file and line if sufficiently severe
        if(stmt.severity >= 2)
            ss_ << "@ " << stmt.code_file << ":" << stmt.code_line << std::endl;

        float ts = std::chrono::duration_cast<std::chrono::duration<float>>(stmt.timestamp).count();
        ss_ << "[" << std::setprecision(6) << std::fixed << ts << "](" << int(stmt.severity) << ") ";
        ss_ << stmt.message;

        entries_.push_back(ss_.str());
        ss_.str("");
    }
    else
        entries_.push_back(stmt.message);
}

void LogFileSink::send_raw(const std::string& message) { entries_.push_back(message); }

static const std::regex s_ansi_regex("\033\\[.+?m"); // matches ANSI codes
static std::string strip_ansi(const std::string& str) { return std::regex_replace(str, s_ansi_regex, ""); }

void LogFileSink::finish()
{
    if(!enabled_)
        return;

    std::ofstream ofs(filename_);

    for(const std::string& entry : entries_)
        ofs << strip_ansi(entry);

    ofs.close();

    std::cout << "\033[1;39mSaved log file: " << k_log_files_style << filename_ << "\n";
}

} // namespace klog
} // namespace kb