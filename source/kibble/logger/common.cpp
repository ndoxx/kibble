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

constexpr uint32_t k_rmask = 0x00FF0000;
constexpr uint32_t k_gmask = 0x0000FF00;
constexpr uint32_t k_bmask = 0x000000FF;
constexpr uint32_t k_rshift = 16;
constexpr uint32_t k_gshift = 8;
constexpr uint32_t k_bshift = 0;

template<> ConsoleColor<true>::ConsoleColor(uint8_t R, uint8_t G, uint8_t B)
{
    color_ = (uint32_t(R) << k_rshift) | (uint32_t(G) << k_gshift) | (uint32_t(B) << k_bshift);
}
template<> ConsoleColor<false>::ConsoleColor(uint8_t R, uint8_t G, uint8_t B)
{
    color_ = (uint32_t(R) << k_rshift) | (uint32_t(G) << k_gshift) | (uint32_t(B) << k_bshift);
}

std::ostream& operator<<(std::ostream& stream, const ConsoleColorClear&)
{
    stream << "\033[0m\033[1;38;2;255;255;255m";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const ConsoleColor<true>& o)
{
    stream << "\033[1;38;2;"
           << uint32_t((o.color_ & k_rmask) >> k_rshift) << ';'
           << uint32_t((o.color_ & k_gmask) >> k_gshift) << ';'
           << uint32_t((o.color_ & k_bmask) >> k_bshift) << 'm';
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const ConsoleColor<false>& o)
{
    stream << "\033[1;48;2;"
           << uint32_t((o.color_ & k_rmask) >> k_rshift) << ';'
           << uint32_t((o.color_ & k_gmask) >> k_gshift) << ';'
           << uint32_t((o.color_ & k_bmask) >> k_bshift) << 'm';
    return stream;
}

} // namespace kb