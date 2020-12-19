#include "cli/terminal.h"
#include "logger/common.h"
#include "math/easings.h"
#include "string/string.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <random>

namespace fs = std::filesystem;

using namespace kb;

void clear_line(size_t count)
{
    for(size_t ii = 0; ii < count; ++ii)
        std::cout << "\033[1A\033[K";
}

void print_bar(float weight, const std::string& name)
{
    auto&& [cols, rows] = cli::get_terminal_size();
    weight = std::clamp(weight, 0.f, 1.f);
    size_t cursor = size_t(std::round(float(cols - 3) * weight));

    // [=======>-------]
    std::string centered_name(su::concat('[', name, ']'));
    su::center(centered_name, int(cols));
    clear_line(2);
    std::cout << centered_name << std::endl;
    std::cout << KF_(col::white) << '[' << KF_(col::mediumturquoise);
    for(size_t ii = 0; ii < cursor; ++ii)
        std::cout << '=';
    std::cout << '>' << KF_(col::ndxorange);
    for(size_t ii = cursor + 1; ii < cols - 2; ++ii)
        std::cout << '-';
    std::cout << KF_(col::white) << ']' << std::endl;
}

void print_colored_rect(float weight, const std::string& name)
{
    auto&& [cols, rows] = cli::get_terminal_size();
    weight = std::clamp(weight, 0.f, 1.f);

    std::string centered_name(su::concat('[', name, ']'));
    su::center(centered_name, int(cols));

    auto color = math::ColorRGBA(weight, 0.f, 1.f - weight);

    clear_line(1);
    std::cout << KB_(math::pack_ARGB(color));
    std::cout << centered_name << std::endl;
}

void test_animate()
{
    using namespace kb::math;

    std::vector<std::pair<float (*)(float), std::string>> easings = {
        {&ease::in_sine, "in_sine"},
        {&ease::out_sine, "out_sine"},
        {&ease::inout_sine, "inout_sine"},
        {&ease::in_exp, "in_exp"},
        {&ease::out_exp, "out_exp"},
        {&ease::inout_exp, "inout_exp"},
        {&ease::in_circ, "in_circ"},
        {&ease::out_circ, "out_circ"},
        {&ease::inout_circ, "inout_circ"},
        {&ease::in_2, "in_2"},
        {&ease::out_2, "out_2"},
        {&ease::inout_2, "inout_2"},
        {&ease::in_3, "in_3"},
        {&ease::out_3, "out_3"},
        {&ease::inout_3, "inout_3"},
        {&ease::in_4, "in_4"},
        {&ease::out_4, "out_4"},
        {&ease::inout_4, "inout_4"},
        {&ease::in_5, "in_5"},
        {&ease::out_5, "out_5"},
        {&ease::inout_5, "inout_5"},
        {&ease::arch_2, "arch_2"},
        {&ease::in_arch_3, "in_arch_3"},
        {&ease::out_arch_3, "out_arch_3"},
        {&ease::inout_arch4, "inout_arch4"},
        {&ease::bell_6, "bell_6"},
        {&ease::in_bounce_bezier_3, "in_bounce_bezier_3"},
        {&ease::out_bounce_bezier_3, "out_bounce_bezier_3"},
        {&ease::inout_bounce_bezier_3, "inout_bounce_bezier_3"},
    };

    float frame_duration_s = 1.f / 60.f;
    float animation_duration_s = 2.f;
    size_t frame_count = size_t(std::round(animation_duration_s / frame_duration_s));
    std::chrono::duration<float> frame_duration(frame_duration_s);

    std::cout << std::endl;

    for(auto&& [pfunc, name] : easings)
    {
        for(size_t ii = 0; ii < frame_count; ++ii)
        {
            float tt = float(ii) / float(frame_count - 1);
            // print_colored_rect((*pfunc)(tt), name);
            print_bar((*pfunc)(tt), name);
            std::this_thread::sleep_for(frame_duration);
        }
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    test_animate();

    return 0;
}