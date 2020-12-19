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
/*
constexpr float test_inout_3(float t) { return easing::crossfade(easing::in_3(t), easing::out_3(t), t); } // DNW
constexpr float test_inout_4(float t) { return easing::crossfade(easing::in_4(t), easing::out_4(t), t); } // DNW
constexpr float test_inout_5(float t) { return easing::crossfade(easing::in_5(t), easing::out_5(t), t); } // DNW
*/

void test_animate()
{
    using namespace kb::math;

    std::vector<std::pair<float (*)(float), std::string>> easings = {
        {&easing::in_2, "in_2"},
        {&easing::out_2, "out_2"},
        {&easing::inout_2, "inout_2"},
        {&easing::in_3, "in_3"},
        {&easing::out_3, "out_3"},
        {&easing::inout_3, "inout_3"},
        {&easing::in_4, "in_4"},
        {&easing::out_4, "out_4"},
        {&easing::inout_4, "inout_4"},
        {&easing::in_5, "in_5"},
        {&easing::out_5, "out_5"},
        {&easing::inout_5, "inout_5"},
        {&easing::arch_2, "arch_2"},
        {&easing::in_arch_3, "in_arch_3"},
        {&easing::out_arch_3, "out_arch_3"},
        {&easing::inout_arch4, "inout_arch4"},
        {&easing::bell_6, "bell_6"},
        {&easing::bounce_bezier_3, "bounce_bezier_3"},
    };

    float frame_duration_s = 1.f / 60.f;
    float animation_duration_s = 1.f;
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

#define FUT__ inout_5

void test_error()
{
    // Error calculation for the lookup table version
    size_t nsamples = 10000;
    float sumsq = 0.f;
    for(size_t ii = 0; ii < nsamples; ++ii)
    {
        float tt = float(ii) / float(nsamples - 1);
        float slow = easing::FUT__(tt);
        float fast = experimental::easing::fast(experimental::easing::Func::FUT__, tt);
        float sqerr = (fast - slow) * (fast - slow);
        sumsq += sqerr;
    }
    sumsq /= float(nsamples);
    std::cout << "Error: " << std::sqrt(sumsq) << std::endl;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    test_animate();
    // test_error();

    return 0;
}