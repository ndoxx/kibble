#include <map>
#include <string>

#include "logger_common.h"

namespace kb
{
namespace klog
{

#if ANSI_3
const std::array<WCC, size_t(MsgType::COUNT)> Style::s_colors = {
    WCC("\033[1;39m"), WCC("\033[1;39m"), WCC("\033[1;39m"), WCC("\033[1;39m"), WCC("\033[1;34m"), WCC("\033[1;33m"),
    WCC("\033[1;31m"), WCC("\033[1;31m"), WCC("\033[1;33m"), WCC("\033[1;32m"), WCC("\033[1;31m"),
};
#else
const std::array<WCC, size_t(MsgType::COUNT)> Style::s_colors = {
    WCC(255, 255, 255), WCC(255, 255, 255), WCC(255, 255, 255), WCC(255, 255, 255),
    WCC(150, 130, 255), WCC(255, 175, 0),   WCC(255, 90, 90),   WCC(255, 0, 0),
    WCC(255, 100, 0),   WCC(0, 255, 0),     WCC(255, 0, 0),
};
#endif

#if ANSI_3
const std::array<std::string, size_t(MsgType::COUNT)> Style::s_icons = {
    "",          "    ",      "     \u21B3 ", " \u2107 ",  " \u2055  ", " \u203C  ",
    " \u2020  ", " \u2021  ", " \u0489  ",    " \u203F  ", " \u2054  ",
};
#else
const std::array<std::string, size_t(MsgType::COUNT)> Style::s_icons = {
    "",
    "    ",
    "     \u21B3 ",
    " \u2107 ",
    "\033[1;48;2;20;10;50m \u2055 \033[1;49m ",
    "\033[1;48;2;50;40;10m \u203C \033[1;49m ",
    "\033[1;48;2;50;10;10m \u2020 \033[1;49m ",
    "\033[1;48;2;50;10;10m \u2021 \033[1;49m ",
    "\033[1;48;2;50;40;10m \u0489 \033[1;49m ",
    "\033[1;48;2;10;50;10m \u203F \033[1;49m ",
    "\033[1;48;2;50;10;10m \u2054 \033[1;49m ",
};
#endif

} // namespace klog

#if ANSI_3
static const std::map<char, std::string> s_COLORMAP = {
    {0, "\033[0m"},      // previous style
    {'p', "\033[1;36m"}, // highlight paths in light blue
    {'n', "\033[1;33m"}, // names and symbols in dark orange
    {'i', "\033[1;33m"}, // instructions in light orange
    {'w', "\033[1;35m"}, // values in light purple
    {'v', "\033[1;32m"}, // important values in green
    {'u', "\033[1;32m"}, // uniforms and attributes in light green
    {'d', "\033[1;33m"}, // default in vivid orange
    {'b', "\033[1;31m"}, // bad things in red
    {'g', "\033[1;32m"}, // good things in green
    {'z', "\033[1;39m"}, // neutral things in white
    {'x', "\033[1;36m"}, // XML nodes in turquoise
    {'h', "\033[1;35m"}, // highlight in pink
    {'s', "\033[1;m97"}, // step / phase
};
#else
static const std::map<char, std::string> s_COLORMAP = {
    {0, "\033[0m"},                    // previous style
    {'p', "\033[1;38;2;0;255;255m"},   // highlight paths in light blue
    {'n', "\033[1;38;2;255;50;0m"},    // names and symbols in dark orange
    {'i', "\033[1;38;2;255;190;10m"},  // instructions in light orange
    {'w', "\033[1;38;2;220;200;255m"}, // values in light purple
    {'v', "\033[1;38;2;153;204;0m"},   // important values in green
    {'u', "\033[1;38;2;0;255;100m"},   // uniforms and attributes in light green
    {'d', "\033[1;38;2;255;100;0m"},   // default in vivid orange
    {'b', "\033[1;38;2;255;0;0m"},     // bad things in red
    {'g', "\033[1;38;2;0;255;0m"},     // good things in green
    {'z', "\033[1;38;2;255;255;255m"}, // neutral things in white
    {'x', "\033[1;38;2;0;206;209m"},   // XML nodes in turquoise
    {'h', "\033[1;38;2;255;51;204m"},  // highlight in pink
    {'s', "\033[1;38;2;0;204;153m"},   // step / phase
};
#endif

WCC::WCC(char cc) : escape(s_COLORMAP.at(cc)) {}

WCC::WCC(const std::string& e) : escape(e) {}

WCC::WCC(uint8_t R, uint8_t G, uint8_t B)
    : escape("\033[1;38;2;" + std::to_string(R) + ";" + std::to_string(G) + ";" + std::to_string(B) + "m")
{}

WCB::WCB(int) : escape("\033[0m") {}

WCB::WCB(const std::string& e) : escape(e) {}

WCB::WCB(uint8_t R, uint8_t G, uint8_t B)
    : escape("\033[1;48;2;" + std::to_string(R) + ";" + std::to_string(G) + ";" + std::to_string(B) + "m")
{}

std::ostream& operator<<(std::ostream& stream, const WCC& wcc)
{
    stream << wcc.escape;
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const WCB& wcb)
{
    stream << wcb.escape;
    return stream;
}

} // namespace kb