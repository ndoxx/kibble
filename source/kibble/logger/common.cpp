#include <map>
#include <string>

#include "logger/common.h"

namespace kb
{
namespace klog
{

const std::array<KF_, size_t(MsgType::COUNT)> Style::s_colors = {
    KF_(255, 255, 255), KF_(255, 255, 255), KF_(255, 255, 255), KF_(255, 255, 255),
    KF_(150, 130, 255), KF_(255, 175, 0),   KF_(255, 90, 90),   KF_(255, 0, 0),
    KF_(255, 100, 0),   KF_(0, 255, 0),     KF_(255, 0, 0),
};

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

} // namespace klog

std::ostream& operator<<(std::ostream& stream, const ConsoleColorClear&)
{
    stream << "\033[0m\033[1;38;2;255;255;255m";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const ConsoleColor<true>& o)
{
    stream << "\033[1;38;2;" << o.color_.r() << ';' << o.color_.g() << ';' << o.color_.b() << 'm';
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const ConsoleColor<false>& o)
{
    stream << "\033[1;48;2;" << o.color_.r() << ';' << o.color_.g() << ';' << o.color_.b() << 'm';
    return stream;
}

} // namespace kb