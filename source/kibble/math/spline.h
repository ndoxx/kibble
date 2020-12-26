#pragma once

/* Spline classes are parameterized by a point type (could be a 2D/3D vector or anything else
 * that is vector-like) and do not depend explicitly on some math library types.
 * Points however must define the usual operations (add/subtract, scalar multiply/divide),
 * and the PointDistance struct must be specialized if HermiteSpline is to be used.
 *
 * Length estimation algorithm stemmed from Andrew Willmott's splines lib
 * https://github.com/andrewwillmott/splines-lib
 * - I use structured bindings instead of passing references
 * - I implemented a generic deCasteljau split algorithm with a recursive template
 *   -> which means I can compute lengths of generic order N Bezier curves
 *
 * At the moment, I only implemented a fixed-size (compile-time) Bezier spline, and
 * a cubic Hermite spline that uses cubic Bezier spline segments. Also, there is the
 * UniformHermiteSpline that is an arc-length reparameterization of a basic HermitSpline.
 * This allows uniform percent length sampling along the curve.
 * Points cannot be edited / added / moved dynamically for now.
 */

#include <array>
#include <cmath>
#include <vector>

#include "../assert/assert.h"
#include "constexpr_math.h"

namespace kb
{
namespace math
{

// Specialize this for HermiteSpline's underlying types
template <typename T> struct PointDistance
{
    static inline float distance(const T& p0, const T& p1)
    {
        (void)p0;
        (void)p1;
        return 0.f;
    }
};

namespace detail
{
// Compile-time generation of the factorial array
template <size_t SIZE> constexpr std::array<int, SIZE> gen_factorial()
{
    std::array<int, SIZE> ret{0};
    std::generate(ret.begin(), ret.end(), [n = 0]() mutable { return factorial(n++); });
    return ret;
}
// Maximum number of factorials to compute
constexpr size_t k_max_fac = 11;
// This array contains factorials from 0 to k_max_fac-1
constexpr auto k_fac = gen_factorial<k_max_fac>();
// Basic linear interpolation utility
template <typename T> inline T lerp(const T& a, const T& b, float alpha) { return (1.f - alpha) * a + alpha * b; }

// Evaluate the DIFF_ORDER derivative of a Bezier curve specified by coeffs at parameter value tt
template <size_t DIFF_ORDER, typename T, typename VecT> T bezier_evaluate(float tt, const VecT& coeffs)
{
    T sum(0);
    float tpow = 1.f;
    for(size_t ii = 0; ii < coeffs.size() - DIFF_ORDER; ++ii)
    {
        size_t diff_coeff = 1;
        if constexpr(DIFF_ORDER > 0)
        {
            for(size_t jj = 0; jj < DIFF_ORDER; ++jj)
                diff_coeff *= ii + jj + 1;
        }

        sum += tpow * float(diff_coeff) * coeffs[size_t(ii + DIFF_ORDER)];
        tpow *= tt;
    }
    return sum;
}

// Compute the polynomial (vector) coefficients of a Bezier curve from its control points
template <typename T, typename VecT> void bezier_coefficients(const VecT& control, VecT& coeff)
{
    const int nn = int(control.size());
    int prod = 1;
    for(int jj = 0; jj < nn; ++jj)
    {
        // Product from m=0 to j-1 of (n-m)
        prod *= (jj > 0) ? nn - jj : 1;

        T sum(0);
        for(int ii = 0; ii <= jj; ++ii)
        {
            int comb = (prod * parity(ii + jj)) / (k_fac[size_t(ii)] * k_fac[size_t(jj - ii)]);
            sum += float(comb) * control[size_t(ii)];
        }
        coeff[size_t(jj)] = sum;
    }
}

// Stateless interpolation using deCasteljau's algorithm
// Parameters: recursion level, free index, interpolation parameter and control points
template <typename T, typename VecT> T deCasteljau(unsigned rr, unsigned ii, float tt, const VecT& points)
{
    if(rr == 0)
        return points[ii];

    T p1 = deCasteljau(rr - 1, ii, tt, points);
    T p2 = deCasteljau(rr - 1, ii + 1, tt, points);

    return p1 * (1.f - tt) + p2 * tt;
}

// Recursive deCasteljau algorithm to Split a Bezier curve into two curves of the same order at arbitrary
// parameter value
template <typename T, size_t SIZE, size_t LEVEL>
void deCasteljauSplit(const std::array<T, SIZE - LEVEL>& points, std::array<T, SIZE>& left, std::array<T, SIZE>& right,
                      float param = 0.5f)
{
    // DeCasteljau's algorithm uses a triangular scheme where for each recursion level,
    // all neighboring points from the previous level (N of them) are lerped, giving rise
    // to N-1 new points. The left split is the left edge of the graph and the right split
    // is the right edge of the graph in reverse order :
    //
    //        Points           LEVEL
    // P0    P1    P2    P3      0
    //    q0    q1    q2         1
    //       r0    r1            2
    //          s0               3
    //
    // left  = [P0, q0, r0, s0]
    // right = [s0, r1, q2, P3]
    //
    // https://pages.mtu.edu/~shene/COURSES/cs3621/NOTES/spline/Bezier/bezier-sub.html

    left[LEVEL] = points.front();
    right[SIZE - LEVEL - 1] = points.back();

    if constexpr(LEVEL == SIZE - 1)
        return;
    else
    {
        std::array<T, SIZE - LEVEL - 1> lerps;
        for(size_t ii = 0; ii < points.size() - 1; ++ii)
            lerps[ii] = lerp(points[ii], points[ii + 1], param);

        deCasteljauSplit<T, SIZE, LEVEL + 1>(lerps, left, right, param);
    }
}

// Binary search the input arc_length lookup table for the target length and return its index.
// The lower_bound argument sets the initial lower bound, it allows to skip a portion
// of the array where we already know we won't find the target.
size_t arclen_binary_search(float target, const std::vector<float>& arc_length, size_t lower_bound = 0)
{
    // Binary search
    size_t lb = lower_bound;
    size_t ub = arc_length.size();
    size_t idx = lb;
    while(lb < ub)
    {
        idx = lb + (((ub - lb) / 2));
        if(arc_length[idx] < target)
            lb = idx + 1;
        else
            ub = idx;
    }
    return (arc_length[idx] > target) ? idx - 1 : idx;
}

// Return value and index of the largest arc length value smaller than target.
// The last_index argument is transmitted to the binary search function so as
// to avoid useless iterations.
std::pair<float, size_t> arclen_remap(float tt, const std::vector<float>& arc_length, size_t last_index = 0)
{
    float target = tt * arc_length.back();
    size_t idx = arclen_binary_search(target, arc_length, last_index);
    if(idx == arc_length.size() - 1)
        return {1.f, idx};

    float len_before = arc_length[idx];
    float len_after = arc_length[idx + 1];
    float len_segment = len_after - len_before;
    float alpha = (target - len_before) / len_segment;
    float ret = (float(idx) + alpha) / float(arc_length.size() - 1);
    return {ret, idx};
}

} // namespace detail

// Stateless interpolation (deCasteljau)
template <typename T> inline T deCasteljau(float tt, const std::vector<T>& points)
{
    return detail::deCasteljau(points.size() - 1, 0, tt, points);
}

// A Bezier spline of order SIZE-1
template <typename T, size_t SIZE> class FixedBezierSpline
{
public:
    FixedBezierSpline(std::array<T, SIZE>&& control_points) : control_(std::move(control_points))
    {
        detail::bezier_coefficients<T>(control_, coeff_);
    }

    // * Access
    // Get the number of control points
    inline size_t get_count() const { return control_.size(); }
    // Get a control point
    inline const T& get_control_point(size_t idx) const
    {
        K_ASSERT(idx < control_.size(), "Index out of bounds.");
        return control_.at(idx);
    }
    // Get first control point
    inline const T& front() const { return control_.front(); }
    // Get last control point
    inline const T& back() const { return control_.back(); }
    // Get all control points
    inline const auto& get_control_points() const { return control_; }

    // * Interpolation functions
    // Return the value along the curve at specified parameter value
    inline T value(float tt) const { return detail::bezier_evaluate<0, T>(tt, coeff_); }
    // Return the first derivative at specified parameter value
    inline T prime(float tt) const { return detail::bezier_evaluate<1, T>(tt, coeff_); }
    // Return the second derivative at specified parameter value
    inline T second(float tt) const { return detail::bezier_evaluate<2, T>(tt, coeff_); }
    // Return the total length of this curve, such that the error is less than input argument
    inline float length(float max_error) const { return length(*this, max_error); }
    // Split this Bezier curve into two curves of the same order
    inline std::pair<FixedBezierSpline, FixedBezierSpline> split(float tt) const
    {
        std::array<T, SIZE> left;
        std::array<T, SIZE> right;
        detail::deCasteljauSplit<T, SIZE, 0>(control_, left, right, tt);
        return {FixedBezierSpline(std::move(left)), FixedBezierSpline(std::move(right))};
    }

private:
    // Return a float pair containing a length estimate and an error
    std::pair<float, float> length_estimate() const
    {
        // Shortest path is from first control point to the last one
        float min_length = PointDistance<T>::distance(control_.front(), control_.back());
        // Longest path is the one that goes through all points in order
        float max_length = 0.f;
        for(size_t ii = 0; ii < control_.size() - 1; ++ii)
            max_length += PointDistance<T>::distance(control_[ii], control_[ii + 1]);
        return {0.5f * (max_length + min_length), 0.5f * (max_length - min_length)};
    }

    // Recursive length computation
    static float length(const FixedBezierSpline& spline, float max_error)
    {
        // While the length estimation error is too big, split the curve and add
        // the length of the two subdivisions
        auto&& [len, error] = spline.length_estimate();
        if(error > max_error)
        {
            auto&& [s0, s1] = spline.split(0.5f);
            return length(s0, max_error) + length(s1, max_error);
        }
        return len;
    }

private:
    std::array<T, SIZE> control_;
    std::array<T, SIZE> coeff_;
};

// A cubic Hermite spline whose segments are expressed as cubic Bezier splines
template <typename T> class HermiteSpline
{
public:
    static constexpr size_t k_min_control_points = 1;

    HermiteSpline(const std::vector<T>& control_points, float tension = 0.f, const T& start_tangent = T(0),
                  const T& end_tangent = T(0))
        : control_(control_points)
    {
        K_ASSERT(control_.size() > k_min_control_points, "There must be at least 2 control points.");

        // Compute tangents (formula for a generic cardinal spline)
        // tension = 0.f  => Catmull-Rom spline
        // tension = 0.5f => Finite-difference spline
        // tension = 1.f  => Null-tangent spline
        std::vector<T> tangents(control_.size(), T(0));
        tangents[0] = start_tangent;
        for(size_t ii = 1; ii < tangents.size() - 1; ++ii)
            tangents[ii] = (1.f - tension) * (control_[ii + 1] - control_[ii - 1]);
        tangents[control_.size() - 1] = end_tangent;

        // Each spline segment is a cubic Bezier spline
        segment_.clear();
        for(size_t ii = 0; ii < control_.size() - 1; ++ii)
        {
            segment_.emplace_back(std::array{control_[ii], control_[ii] + tangents[ii] / 3.f,
                                             control_[ii + 1] - tangents[ii + 1] / 3.f, control_[ii + 1]});
        }
    }
    HermiteSpline() : HermiteSpline({T(0), T(0)}) {}

    // Return the total length of this curve, such that the error is less than input argument
    float length(float max_error) const
    {
        float sum = 0.f;
        for(const auto& seg : segment_)
            sum += seg.length(max_error);
        return sum;
    }

    // * Interpolation functions
    // Return the value along the curve at specified parameter value
    inline T value(float tt) const
    {
        auto idx = locate_segment(tt);
        return segment_[idx].value(to_local_space(tt, idx));
    }
    // Return the first derivative at specified parameter value
    inline T prime(float tt) const
    {
        auto idx = locate_segment(tt);
        return segment_[idx].prime(to_local_space(tt, idx));
    }
    // Return the second derivative at specified parameter value
    inline T second(float tt) const
    {
        auto idx = locate_segment(tt);
        return segment_[idx].second(to_local_space(tt, idx));
    }

protected:
    // Remap parameter to local cubic spline parameter space
    inline float to_local_space(float tt, size_t idx) const { return tt * float(control_.size() - 1) - float(idx); }
    // Return segment index associated to subinterval containing tt.
    inline size_t locate_segment(float tt) const
    {
        // Clamp index if tt is out of bounds. A value of exactly 1.f would yield
        // an index out of bounds, so the upper bound is set to the number just before 1.f
        tt = std::clamp(tt, 0.f, std::nexttoward(1.f, -1));
        return size_t(std::floor(float(control_.size() - 1) * tt));
    }

protected:
    std::vector<T> control_;
    std::vector<FixedBezierSpline<T, 4>> segment_;
};

// Arc-length parameterized Hermite spline
template <typename T> class UniformHermiteSpline : public HermiteSpline<T>
{
public:
    UniformHermiteSpline(const std::vector<T>& control_points, size_t max_lookup = 64, float tension = 0.f,
                         const T& start_tangent = T(0), const T& end_tangent = T(0))
        : HermiteSpline<T>(control_points, tension, start_tangent, end_tangent)
    {
        calculate_lookup_iterative(max_lookup);
    }
    UniformHermiteSpline() : UniformHermiteSpline({T(0), T(0)}) {}

    // * Interpolation functions
    // BEWARE: these are NON-VIRTUAL overrides
    // Return arc-length parameterized value
    inline T value(float uu) const { return HermiteSpline<T>::value(arclen_remap(uu)); }
    // Return arc-length parameterized first derivative
    inline T prime(float uu) const { return HermiteSpline<T>::prime(arclen_remap(uu)); }
    // Return arc-length parameterized second derivative
    inline T second(float uu) const { return HermiteSpline<T>::second(arclen_remap(uu)); }

protected:
    using HermiteSpline<T>::control_;

private:
    // Compute the remapping lookup table, its size will be max_iter
    void calculate_lookup_iterative(size_t max_iter)
    {
        // Sample the spline and estimate the cumulative lengths of each subintervals
        std::vector<float> arc_length;
        arc_length.resize(max_iter);
        float arclen = 0.f;
        T prev = control_[0];
        for(size_t ii = 0; ii < max_iter; ++ii)
        {
            float tt = float(ii) / float(max_iter - 1);
            T point = HermiteSpline<T>::value(tt);
            arclen += PointDistance<T>::distance(point, prev);
            arc_length[ii] = arclen;
            prev = point;
        }

        // Inverse the arc length function : compute parameter value as a function of arc length
        size_t last_index = 0;
        arc_length_inverse_.resize(max_iter);
        for(size_t ii = 0; ii < max_iter; ++ii)
        {
            float uu = float(ii) / float(max_iter - 1);
            // Because the length array is monotonically increasing, we know we won't find our target
            // before the last_index, then we can cut down costs by providing last_index to the remap func.
            auto&& [param, idx] = detail::arclen_remap(uu, arc_length, last_index);
            arc_length_inverse_[ii] = param;
            last_index = idx;
        }
    }

    // Estimate a parameter value such that uu represents the length fraction along the curve
    float arclen_remap(float uu) const
    {
        // Sample the lookup table
        uu = std::clamp(uu, 0.f, std::nexttoward(1.f, -1));
        size_t idx = size_t(std::floor(float(arc_length_inverse_.size() - 1) * uu));
        if(idx == arc_length_inverse_.size() - 1)
            return arc_length_inverse_.back();
        float alpha = float(arc_length_inverse_.size() - 1) * uu - float(idx);
        return std::lerp(arc_length_inverse_[idx], arc_length_inverse_[idx + 1], alpha);
    }

private:
    std::vector<float> arc_length_inverse_;
};

} // namespace math
} // namespace kb