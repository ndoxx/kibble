#include "logger/sink.h"
#include "logger/common.h"
#include "net/tcp_connector.h"
#include "net/tcp_stream.h"
#include "string/string.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string_view>

namespace kb
{
namespace log_deprec
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
    if (stmt.msg_type != MsgType::RAW)
    {
        // Show file and line if sufficiently severe
        if (stmt.severity >= 2)
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

void ConsoleSink::send_raw(const std::string& message)
{
    std::cout << message;
}

LogFileSink::LogFileSink(const std::string& filename) : filename_(filename), out_(filename)
{
}

static const std::regex s_ansi_regex("\033\\[.+?m"); // matches ANSI codes
static std::string strip_ansi(const std::string& str)
{
    return std::regex_replace(str, s_ansi_regex, "");
}

void LogFileSink::send(const LogStatement& stmt, const LogChannel&)
{
    if (stmt.msg_type != MsgType::RAW)
    {
        // Show file and line if sufficiently severe
        if (stmt.severity >= 2)
            out_ << "@ " << stmt.code_file << ":" << stmt.code_line << std::endl;

        float ts = std::chrono::duration_cast<std::chrono::duration<float>>(stmt.timestamp).count();
        out_ << "[" << std::setprecision(6) << std::fixed << ts << "](" << int(stmt.severity) << ") ";
        out_ << strip_ansi(stmt.message);
    }
    else
        out_ << strip_ansi(stmt.message);
}

void LogFileSink::send_raw(const std::string& message)
{
    out_ << message << std::endl;
}

void LogFileSink::finish()
{
    if (!enabled_)
        return;

    out_.close();
    std::cout << "\033[1;39mSaved log file: " << k_log_files_style << filename_ << "\n";
}

NetSink::~NetSink()
{
    if (stream_)
    {
        // Notify server before closing connection
        stream_->send("{\"action\":\"disconnect\"}");
        delete stream_;
    }
}

bool NetSink::connect(const std::string& server, uint16_t port)
{
    server_ = server;
    stream_ = kb::net::TCPConnector::connect(server, port);
    return (stream_ != nullptr);
}

void NetSink::on_attach()
{
    if (stream_ != nullptr)
    {
        std::stringstream ss;
        // Notify new connection
        ss << "{\"action\":\"connect\", "
           << "\"peer_ip\":\"" << stream_->get_peer_ip() << "\", "
           << "\"peer_port\":\"" << uint32_t(stream_->get_peer_port()) << "\"}";
        stream_->send(ss.str());

        // Send subscribed channels to server
        ss.str("");
        ss << "{\"action\":\"set_channels\", \"channels\":[";
        for (size_t ii = 0; ii < subscriptions_.size(); ++ii)
        {
            const auto& desc = subscriptions_[ii];
            ss << '\"' << desc.name << '\"';
            if (ii < subscriptions_.size() - 1)
                ss << ',';
        }
        ss << "]}";
        stream_->send(ss.str());
    }
}

void NetSink::send(const kb::log_deprec::LogStatement& stmt, const kb::log_deprec::LogChannel& chan)
{
    // Send JSON formatted message
    std::stringstream ss;
    ss << "{\"action\":\"msg\", \"channel\":\"" << chan.name << "\", \"type\":\"" << uint32_t(stmt.msg_type)
       << "\", \"severity\":\"" << uint32_t(stmt.severity) << "\", \"timestamp\":\""
       << std::chrono::duration_cast<std::chrono::duration<float>>(stmt.timestamp).count() << "\", \"line\":\""
       << stmt.code_line << "\", \"file\":\"" << stmt.code_file << "\", \"message\":\""
       << su::base64_encode(stmt.message + "\033[0m") << "\"}";
    stream_->send(ss.str());
}

void NetSink::send_raw(const std::string& message)
{
    stream_->send(message);
}

} // namespace log_deprec
} // namespace kb