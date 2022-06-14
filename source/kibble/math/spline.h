#pragma once

/**
 * @file spline.h
 * @brief Spline classes and utilities.
 *
 * Spline classes are parameterized by a point type (could be a 2D/3D vector or anything else
 * that is vector-like) and do not depend explicitly on some math library types.
 * Points however must define the usual operations (add/subtract, scalar multiply/divide),
 * and the PointDistance struct must be specialized if HermiteSpline is to be used.
 *
 * At the moment, I only implemented a fixed-size (compile-time) Bezier spline, and
 * a cubic Hermite spline that uses cubic Bezier spline segments. Also, there is the
 * UniformHermiteSpline that is an arc-length reparameterization of a basic HermitSpline.
 * This allows uniform percent length sampling along the curve.
 * Points cannot be edited / added / moved dynamically for now.
 *
 * @author ndx
 * @version 0.1
 * @date 2021-11-07
 *
 * @copyright Copyright (c) 2021
 *
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

/**
 * @brief This template needs to be specialized for the HermiteSpline's underlying types.
 *
 * @tparam T Point type
 */
template <typename T>
struct PointDistance
{
    /**
     * @brief Return the distance between two points
     *
     * @param p0
     * @param p1
     * @return float
     */
    static inline float distance(const T &p0, const T &p1)
    {
        (void)p0;
        (void)p1;
        return 0.f;
    }
};

namespace detail
{
/**
 * @internal
 * @brief Compile-time generation of the factorial array
 *
 * @tparam SIZE array size
 * @return an array containing factorials
 */
template <size_t SIZE>
constexpr std::array<int, SIZE> gen_factorial()
{
    std::array<int, SIZE> ret{0};
    std::generate(ret.begin(), ret.end(), [n = 0]() mutable { return factorial(n++); });
    return ret;
}

/// Maximum number of factorials to compute
constexpr size_t k_max_fac = 11;

/// This array contains factorials from 0 to k_max_fac-1
constexpr auto k_fac = gen_factorial<k_max_fac>();

/**
 * @internal
 * @brief Basic linear interpolation utility
 *
 * @tparam T Point type
 * @param a first value
 * @param b second value
 * @param alpha interpolation parameter
 * @return T
 */
template <typename T>
inline T lerp(const T &a, const T &b, float alpha)
{
    return (1.f - alpha) * a + alpha * b;
}

/**
 * @internal
 * @brief Evaluate the n-th order derivative of a Bezier curve specified by a list of coefficients at a given parameter
 * value.
 *
 * @tparam DIFF_ORDER Derivative order
 * @tparam T Point type
 * @tparam VecT Type of coefficient vector
 * @param tt Parameter value
 * @param coeffs List of Bezier coefficients
 * @return DIFF_ORDER-th derivative value at point tt
 */
template <size_t DIFF_ORDER, typename T, typename VecT>
T bezier_evaluate(float tt, const VecT &coeffs)
{
    T sum(0);
    float tpow = 1.f;
    for (size_t ii = 0; ii < coeffs.size() - DIFF_ORDER; ++ii)
    {
        size_t diff_coeff = 1;
        if constexpr (DIFF_ORDER > 0)
        {
            for (size_t jj = 0; jj < DIFF_ORDER; ++jj)
                diff_coeff *= ii + jj + 1;
        }

        sum += tpow * float(diff_coeff) * coeffs[size_t(ii + DIFF_ORDER)];
        tpow *= tt;
    }
    return sum;
}

/**
 * @internal
 * @brief Compute the polynomial (vector) coefficients of a Bezier curve from its control points.
 *
 * @tparam T Point type
 * @tparam VecT Type of coefficient vector
 * @param control list of control points
 * @param coeff output argument where the coefficients will be pushed
 */
template <typename T, typename VecT>
void bezier_coefficients(const VecT &control, VecT &coeff)
{
    const int nn = int(control.size());
    int prod = 1;
    for (int jj = 0; jj < nn; ++jj)
    {
        // Product from m=0 to j-1 of (n-m)
        prod *= (jj > 0) ? nn - jj : 1;

        T sum(0);
        for (int ii = 0; ii <= jj; ++ii)
        {
            int comb = (prod * parity(ii + jj)) / (k_fac[size_t(ii)] * k_fac[size_t(jj - ii)]);
            sum += float(comb) * control[size_t(ii)];
        }
        coeff[size_t(jj)] = sum;
    }
}

/**
 * @internal
 * @brief Recursive stateless interpolation using deCasteljau's algorithm.
 *
 * @tparam T Point type
 * @tparam VecT Type of coefficient vector
 * @param rr recursion level
 * @param ii free index
 * @param tt interpolation parameter
 * @param points control points
 * @return T value of the interpolation at point tt
 */
template <typename T, typename VecT>
T deCasteljau(unsigned rr, unsigned ii, float tt, const VecT &points)
{
    if (rr == 0)
        return points[ii];

    T p1 = deCasteljau(rr - 1, ii, tt, points);
    T p2 = deCasteljau(rr - 1, ii + 1, tt, points);

    return p1 * (1.f - tt) + p2 * tt;
}

/**
 * @internal
 * @brief Recursive deCasteljau split for a Bezier curve into two curves of the same order at arbitrary
 * parameter value
 *
 * @tparam T Point type
 * @tparam SIZE Size of the splits
 * @tparam LEVEL template recursion index
 * @param points control points
 * @param left left split output argument
 * @param right right split output argument
 * @param param parameter value in [0,1]
 */
template <typename T, size_t SIZE, size_t LEVEL>
void deCasteljauSplit(const std::array<T, SIZE - LEVEL> &points, std::array<T, SIZE> &left, std::array<T, SIZE> &right,
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

    if constexpr (LEVEL == SIZE - 1)
        return;
    else
    {
        std::array<T, SIZE - LEVEL - 1> lerps;
        for (size_t ii = 0; ii < points.size() - 1; ++ii)
            lerps[ii] = lerp(points[ii], points[ii + 1], param);

        deCasteljauSplit<T, SIZE, LEVEL + 1>(lerps, left, right, param);
    }
}

/**
 * @internal
 * @brief Binary search the input arc_length lookup table for the target length and return its index.
 * The lower_bound argument sets the initial lower bound, it allows to skip a portion of the array where we already know
 * we won't find the target.
 *
 * @param target target arc-length to find the index of
 * @param arc_length input arc-length table
 * @param lower_bound initial lower bound
 * @return size_t index of the target arc-length
 */
size_t arclen_binary_search(float target, const std::vector<float> &arc_length, size_t lower_bound = 0)
{
    // Binary search
    size_t lb = lower_bound;
    size_t ub = arc_length.size();
    size_t idx = lb;
    while (lb < ub)
    {
        idx = lb + (((ub - lb) / 2));
        if (arc_length[idx] < target)
            lb = idx + 1;
        else
            ub = idx;
    }
    return (arc_length[idx] > target) ? idx - 1 : idx;
}

/**
 * @internal
 * @brief Return value and index of the largest arc length value smaller than target.
 * The last_index argument is transmitted to the binary search function as an initial lower bound so as to avoid useless
 * iterations.
 *
 * @param tt parameter value
 * @param arc_length arc-length table
 * @param last_index last index obtained through iterated calls to this function
 * @return a pair containing the arc-length value and table index
 */
std::pair<float, size_t> arclen_remap(float tt, const std::vector<float> &arc_length, size_t last_index = 0)
{
    float target = std::clamp(tt, 0.f, 1.f) * arc_length.back();
    size_t idx = arclen_binary_search(target, arc_length, last_index);
    if (idx == arc_length.size() - 1)
        return {1.f, idx};

    float len_before = arc_length[idx];
    float len_after = arc_length[idx + 1];
    float len_segment = len_after - len_before;
    float alpha = (target - len_before) / len_segment;
    float ret = (float(idx) + alpha) / float(arc_length.size() - 1);
    return {ret, idx};
}

} // namespace detail

/**
 * @brief Computes the point on the Bezier curve at a specified parameter value.
 * This is a stateless deCasteljau interpolation.
 *
 * @tparam T Point type
 * @param tt parameter value
 * @param points control points
 * @return point on the curve at tt
 */
template <typename T>
inline T deCasteljau(float tt, const std::vector<T> &points)
{
    return detail::deCasteljau(points.size() - 1, 0, tt, points);
}

/**
 * @brief Fixed-size compile-time Bezier spline / interpolator.
 * The order of the spline is SIZE-1.
 *
 * @tparam T point type
 * @tparam SIZE number of control points
 */
template <typename T, size_t SIZE>
class FixedBezierSpline
{
public:
    /**
     * @brief Construct a new Fixed Bezier Spline from a list of control points.
     *
     * @param control_points the list of control points
     */
    FixedBezierSpline(std::array<T, SIZE> &&control_points) : control_(std::move(control_points))
    {
        detail::bezier_coefficients<T>(control_, coeff_);
    }

    // * Access
    /**
     * @brief Get the number of control points.
     *
     * @return size_t
     */
    inline size_t get_count() const
    {
        return control_.size();
    }

    /**
     * @brief Get a control point by index.
     *
     * @param idx index in the control point list
     * @return const T&
     */
    inline const T &get_control_point(size_t idx) const
    {
        K_ASSERT(idx < control_.size(), "Index out of bounds.");
        return control_.at(idx);
    }

    /**
     * @brief Get the first control point.
     *
     * @return const T&
     */
    inline const T &front() const
    {
        return control_.front();
    }

    /**
     * @brief Get the last control point.
     *
     * @return const T&
     */
    inline const T &back() const
    {
        return control_.back();
    }

    /**
     * @brief Get all control points.
     *
     * @return const auto&
     */
    inline const auto &get_control_points() const
    {
        return control_;
    }

    // * Interpolation functions
    /**
     * @brief Return the value along the curve at the specified parameter value.
     *
     * @param tt parameter value
     * @return T
     */
    inline T value(float tt) const
    {
        return detail::bezier_evaluate<0, T>(tt, coeff_);
    }

    /**
     * @brief Return the value of the first derivative at the specified parameter value.
     *
     * @param tt parameter value
     * @return T
     */
    inline T prime(float tt) const
    {
        return detail::bezier_evaluate<1, T>(tt, coeff_);
    }

    /**
     * @brief Return the value of the second derivative at the specified parameter value.
     *
     * @param tt parameter value
     * @return T
     */
    inline T second(float tt) const
    {
        return detail::bezier_evaluate<2, T>(tt, coeff_);
    }

    /**
     * @brief Return the total length of this curve, such that the error is less than the input argument.
     *
     * @param max_error Maximum allowable error for the length computation
     * @return float Length estimation
     */
    inline float length(float max_error) const
    {
        return length(*this, max_error);
    }

    /**
     * @brief Split this Bezier curve into two curves of the same order
     *
     * @param tt split point, 0.5 to split in the middle
     * @return the left and right splits
     */
    inline std::pair<FixedBezierSpline, FixedBezierSpline> split(float tt) const
    {
        std::array<T, SIZE> left;
        std::array<T, SIZE> right;
        detail::deCasteljauSplit<T, SIZE, 0>(control_, left, right, tt);
        return {FixedBezierSpline(std::move(left)), FixedBezierSpline(std::move(right))};
    }

private:
    /**
     * @internal
     * @brief Return a float pair containing a length estimate and an error.
     * This estimate is good when the curve is close to linear.
     *
     * @return std::pair<float, float>
     */
    std::pair<float, float> length_estimate() const
    {
        // Shortest path is from first control point to the last one
        float min_length = PointDistance<T>::distance(control_.front(), control_.back());
        // Longest path is the one that goes through all points in order
        float max_length = 0.f;
        for (size_t ii = 0; ii < control_.size() - 1; ++ii)
            max_length += PointDistance<T>::distance(control_[ii], control_[ii + 1]);
        return {0.5f * (max_length + min_length), 0.5f * (max_length - min_length)};
    }

    /**
     * @brief Compute the length of a Bezier spline recursively.
     * The curve is subdivided until the segments are linear enough that the length_estimate() will give a good
     * approximation for each of them, then all the contributions are summed up.
     *
     * The length estimation algorithm stemmed from Andrew Willmott's splines lib
     * (https://github.com/andrewwillmott/splines-lib). The main difference is that I implemented a generic deCasteljau
     * split algorithm with a recursive template, which means I can potentially compute lengths of generic order N
     * Bezier curves
     *
     * @param spline The spline whose length is to be evaluated
     * @param max_error Maximum allowable error for the length computation
     * @return float
     */
    static float length(const FixedBezierSpline &spline, float max_error)
    {
        // While the length estimation error is too big, split the curve and add
        // the length of the two subdivisions
        auto &&[len, error] = spline.length_estimate();
        if (error > max_error)
        {
            auto &&[s0, s1] = spline.split(0.5f);
            return length(s0, max_error) + length(s1, max_error);
        }
        return len;
    }

private:
    std::array<T, SIZE> control_;
    std::array<T, SIZE> coeff_;
};

/**
 * @brief A cubic Hermite spline whose segments are expressed as cubic Bezier splines.
 * A cubic Hermite spline always passes through its control points. Each spline segment (between two control points) is
 * modeled by a FixedBezierSpline of size 4. The 4 points of each segment are comprised of the two ends and two
 * additional control points that constrain the tangents at both ends. A size of 4 means that each segment is of order
 * 3, hence the name *cubic* Hermite spline. The tangents are computed thanks to a formula for generic cardinal splines.
 *
 * @tparam T Point type
 */
template <typename T>
class HermiteSpline
{
public:
    static constexpr size_t k_min_control_points = 1;

    /**
     * @brief Construct a Hermite Spline from control points and a tension parameter.
     * The tension parameter controls the tangents length in some sense. Only the tangents at the start and at the end
     * are left free by the cardinal spline formula, so they can be specified as arguments.
     *
     * @param control_points list of control points
     * @param tension tension parameter:
     * - tension = 0.5f  => *Catmull-Rom spline*
     * - tension = 1.f  => *Null-tangent spline*
     * @param start_tangent tangent at the first control point
     * @param end_tangent tangent at the last control point
     */
    HermiteSpline(const std::vector<T> &control_points, float tension = 0.f, const T &start_tangent = T(0),
                  const T &end_tangent = T(0))
        : control_(control_points)
    {
        K_ASSERT(control_.size() > k_min_control_points, "There must be at least 2 control points.");

        // Compute tangents (formula for a generic cardinal spline)
        std::vector<T> tangents(control_.size(), T(0));
        tangents[0] = start_tangent;
        for (size_t ii = 1; ii < tangents.size() - 1; ++ii)
            tangents[ii] = (1.f - tension) * (control_[ii + 1] - control_[ii - 1]);
        tangents[control_.size() - 1] = end_tangent;

        // Each spline segment is a cubic Bezier spline
        segment_.clear();
        for (size_t ii = 0; ii < control_.size() - 1; ++ii)
        {
            segment_.emplace_back(std::array{control_[ii], control_[ii] + tangents[ii] / 3.f,
                                             control_[ii + 1] - tangents[ii + 1] / 3.f, control_[ii + 1]});
        }
    }

    /**
     * @brief Default to an empty Hermite spline.
     *
     */
    HermiteSpline() : HermiteSpline({T(0), T(0)})
    {
    }

    /**
     * @brief Return the total length of this curve, such that the error is less than input argument.
     *
     * @param max_error maximum allowable error for the length computation
     * @return float
     */
    float length(float max_error) const
    {
        float sum = 0.f;
        for (const auto &seg : segment_)
            sum += seg.length(max_error);
        return sum;
    }

    // * Interpolation functions
    /**
     * @brief Return the value along the curve at specified parameter value.
     *
     * @param tt parameter value
     * @return T point type
     */
    inline T value(float tt) const
    {
        auto idx = locate_segment(tt);
        return segment_[idx].value(to_local_space(tt, idx));
    }

    /**
     * @brief Return the first derivative at specified parameter value.
     *
     * @param tt parameter value
     * @return T point type
     */
    inline T prime(float tt) const
    {
        auto idx = locate_segment(tt);
        return segment_[idx].prime(to_local_space(tt, idx));
    }

    /**
     * @brief Return the second derivative at specified parameter value
     *
     * @param tt parameter value
     * @return T point type
     */
    inline T second(float tt) const
    {
        auto idx = locate_segment(tt);
        return segment_[idx].second(to_local_space(tt, idx));
    }

protected:
    /**
     * @internal
     * @brief Remap parameter to local cubic spline parameter space.
     *
     * @param tt global spline parameter between 0 and 1
     * @param idx index of the spline containing tt
     * @return local spline parameter value
     * @see locate_segment()
     */
    inline float to_local_space(float tt, size_t idx) const
    {
        return tt * float(control_.size() - 1) - float(idx);
    }

    /**
     * @internal
     * @brief Return segment index associated to subinterval containing tt.
     *
     * @param tt global spline parameter value
     * @return size_t index of the spline containing tt
     */
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

/**
 * @brief Arc-length parameterized Hermite spline.
 * This Hermite spline class is a cubic Hermite spline with the additional property that it can be uniformly percent
 * length sampled. It is better to use this type of spline in anything procedurally generated, so the geometry segments
 * of the final object do not appear to change length non-linearly as the interpolation parameter progresses.
 *
 * @tparam T point type
 */
template <typename T>
class UniformHermiteSpline : public HermiteSpline<T>
{
public:
    /**
     * @brief Construct a new Uniform Hermite Spline.
     * There is no closed form formula for the arc-length reparameterization of any spline of order greater than 2, due
     * to the presence of elliptic integrals. The best we can do is to sample a length estimation function along the
     * curve and numerically invert it using a table. The only parameter that is not directly inherited from
     * HermiteSpline is the size of this arc-length table.
     *
     * @param control_points list of control points
     * @param max_lookup size of the arc-length table
     * @param tension tension parameter:
     * - tension = 0.f  => *Catmull-Rom spline*
     * - tension = 0.5f => *Finite-difference spline*
     * - tension = 1.f  => *Null-tangent spline*
     * @param start_tangent tangent at the first control point
     * @param end_tangent tangent at the last control point
     *
     */
    UniformHermiteSpline(const std::vector<T> &control_points, size_t max_lookup = 64, float tension = 0.f,
                         const T &start_tangent = T(0), const T &end_tangent = T(0))
        : HermiteSpline<T>(control_points, tension, start_tangent, end_tangent)
    {
        calculate_lookup_iterative(max_lookup);
    }
    UniformHermiteSpline() : UniformHermiteSpline({T(0), T(0)})
    {
    }

    // * Interpolation functions
    /**
     * @brief Return the arc-length parameterized value.
     * @warning This is a NON-VIRTUAL override
     *
     * @param uu percent length parameter value, between 0 and 1
     * @return T spline value at that parameter value
     */
    inline T value(float uu) const
    {
        return HermiteSpline<T>::value(arclen_remap(uu));
    }

    /**
     * @brief Return arc-length parameterized first derivative.
     * @warning This is a NON-VIRTUAL override
     *
     * @param uu percent length parameter value, between 0 and 1
     * @return T spline value at that parameter value
     */
    inline T prime(float uu) const
    {
        return HermiteSpline<T>::prime(arclen_remap(uu));
    }

    /**
     * @brief Return arc-length parameterized second derivative.
     * @warning This is a NON-VIRTUAL override
     *
     * @param uu percent length parameter value, between 0 and 1
     * @return T spline value at that parameter value
     */
    inline T second(float uu) const
    {
        return HermiteSpline<T>::second(arclen_remap(uu));
    }

protected:
    using HermiteSpline<T>::control_;

private:
    /**
     * @internal
     * @brief Compute the remapping lookup table, its size will be max_iter.
     *
     * @param max_iter
     */
    void calculate_lookup_iterative(size_t max_iter)
    {
        // Sample the spline and estimate the cumulative lengths of each subintervals
        std::vector<float> arc_length;
        arc_length.resize(max_iter);
        float arclen = 0.f;
        T prev = control_[0];
        for (size_t ii = 0; ii < max_iter; ++ii)
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
        for (size_t ii = 0; ii < max_iter; ++ii)
        {
            float uu = float(ii) / float(max_iter - 1);
            // Because the length array is monotonically increasing, we know we won't find our target
            // before the last_index, then we can cut down costs by providing last_index to the remap func.
            auto &&[param, idx] = detail::arclen_remap(uu, arc_length, last_index);
            arc_length_inverse_[ii] = param;
            last_index = idx;
        }
    }

    /**
     * @brief Estimate a parameter value such that uu represents the length fraction along the curve.
     *
     * @param uu length fraction parameter
     * @return float parameter value
     */
    float arclen_remap(float uu) const
    {
        // Sample the lookup table
        uu = std::clamp(uu, 0.f, std::nexttoward(1.f, -1));
        size_t idx = size_t(std::floor(float(arc_length_inverse_.size() - 1) * uu));
        if (idx == arc_length_inverse_.size() - 1)
            return arc_length_inverse_.back();
        float alpha = float(arc_length_inverse_.size() - 1) * uu - float(idx);
        return std::lerp(arc_length_inverse_[idx], arc_length_inverse_[idx + 1], alpha);
    }

private:
    std::vector<float> arc_length_inverse_;
};

} // namespace math
} // namespace kb