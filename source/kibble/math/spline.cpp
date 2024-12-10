#include "kibble/math/spline.h"

namespace kb::math::detail
{

size_t arclen_binary_search(float s_target, const std::vector<float>& arc_length, size_t lower_bound)
{
    // Binary search
    size_t lb = lower_bound;
    size_t ub = arc_length.size();
    size_t idx = lb;
    while (lb < ub)
    {
        idx = lb + (((ub - lb) / 2));
        if (arc_length[idx] < s_target)
        {
            lb = idx + 1;
        }
        else
        {
            ub = idx;
        }
    }
    return (arc_length[idx] > s_target) ? idx - 1 : idx;
}

std::pair<float, size_t> arclen_inverse(float uu, const std::vector<float>& arc_length, size_t lower_bound)
{
    // Arclength from normalized parameter value, total length is the last element of the prefix sum
    float target = std::clamp(uu, 0.f, 1.f) * arc_length.back();
    // Get the index of the largest arclength value that is smaller than our target value
    size_t idx = arclen_binary_search(target, arc_length, lower_bound);
    if (idx == arc_length.size() - 1)
    {
        return {1.f, idx};
    }

    // The distance covered in the LUT by the binary search algorithm is a measure of the inverse of the arc length
    // By construction, we know the target length is located between these two:
    float len_before = arc_length[idx];
    float len_after = arc_length[idx + 1];
    // Compute a fractional part that tells us exactly where it is located in this interval
    float len_segment = len_after - len_before;
    float frac = (target - len_before) / len_segment;
    // Estimate parameter value at target arclength
    float ret = (float(idx) + frac) / float(arc_length.size() - 1);
    return {ret, idx};
}

} // namespace kb::math::detail