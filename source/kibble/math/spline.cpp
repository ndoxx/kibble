#include "kibble/math/spline.h"

namespace kb::math::detail
{

size_t arclen_binary_search(float target, const std::vector<float>& arc_length, size_t lower_bound)
{
    // Binary search
    size_t lb = lower_bound;
    size_t ub = arc_length.size();
    size_t idx = lb;
    while (lb < ub)
    {
        idx = lb + (((ub - lb) / 2));
        if (arc_length[idx] < target)
        {
            lb = idx + 1;
        }
        else
        {
            ub = idx;
        }
    }
    return (arc_length[idx] > target) ? idx - 1 : idx;
}

std::pair<float, size_t> arclen_remap(float tt, const std::vector<float>& arc_length, size_t last_index)
{
    float target = std::clamp(tt, 0.f, 1.f) * arc_length.back();
    size_t idx = arclen_binary_search(target, arc_length, last_index);
    if (idx == arc_length.size() - 1)
    {
        return {1.f, idx};
    }

    float len_before = arc_length[idx];
    float len_after = arc_length[idx + 1];
    float len_segment = len_after - len_before;
    float alpha = (target - len_before) / len_segment;
    float ret = (float(idx) + alpha) / float(arc_length.size() - 1);
    return {ret, idx};
}

} // namespace kb::math::detail