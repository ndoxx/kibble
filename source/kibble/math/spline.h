#pragma once

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
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
template <size_t SIZE> constexpr std::array<int, SIZE> gen_factorial()
{
    std::array<int, SIZE> ret{0};
    std::generate(ret.begin(), ret.end(), [n = 0]() mutable { return factorial(n++); });
    return ret;
}
constexpr size_t k_max_fac = 11;
constexpr auto k_fac = gen_factorial<k_max_fac>();

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

// Return index of the largest arc length value smaller than target
template <typename T> size_t arclen_binary_search(float target, const std::vector<float>& arc_length)
{
    // Binary search
    size_t lb = 0;
    size_t ub = arc_length.size();
    size_t idx = 0;
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

template <typename T> float arclen_parameterize(float tt, const std::vector<float>& arc_length)
{
    float target = tt * arc_length.back();
    size_t idx = arclen_binary_search<T>(target, arc_length);
    size_t nextidx = (idx + 1 > arc_length.size() - 1) ? arc_length.size() - 1 : idx + 1;

    float len_before = arc_length[idx];
    float len_after = arc_length[nextidx];
    float len_segment = len_after - len_before;
    float alpha = (target - len_before) / len_segment;
    return (float(idx) + alpha) / float(arc_length.size() - 1);
}

} // namespace detail

// Stateless interpolation (deCasteljau)
template <typename T> inline T deCasteljau(float tt, const std::vector<T>& points)
{
    return detail::deCasteljau(points.size() - 1, 0, tt, points);
}

template <typename T> class BezierSpline
{
public:
    BezierSpline(const std::vector<T>& control_points) : control_(control_points)
    {
        K_ASSERT(control_.size() > 2, "There must be at least 3 control points.");
        K_ASSERT(control_.size() < detail::k_max_fac, "Maximum number of control points exceeded.");
        coeff_.resize(control_.size());
        detail::bezier_coefficients<T>(control_, coeff_);
    }
    BezierSpline() : BezierSpline({T(0), T(0), T(0)}) {}
    ~BezierSpline() = default;

    // * Access
    // Set all control points
    bool set_control_points(const std::vector<T>& control_points)
    {
        if(control_points.size() > 2 && control_points.size() < detail::k_max_fac)
        {
            control_ = control_points;
            coeff_.resize(control_.size());
            detail::bezier_coefficients<T>(control_, coeff_);
            return true;
        }
        return false;
    }
    // Add a new control point
    bool add(const T& point)
    {
        if(control_.size() + 1 < detail::k_max_fac)
        {
            control_.push_back(point);
            coeff_.resize(control_.size());
            detail::bezier_coefficients<T>(control_, coeff_);
            return true;
        }
        return false;
    }
    // Insert a new control point at specified index
    bool insert(size_t idx, const T& point)
    {
        if(idx < control_.size() && control_.size() + 1 < detail::k_max_fac)
        {
            control_.insert(control_.begin() + long(idx), point);
            coeff_.resize(control_.size());
            detail::bezier_coefficients<T>(control_, coeff_);
            return true;
        }
        return false;
    }
    // Remove control point
    bool remove(size_t idx)
    {
        if(idx < control_.size() && control_.size() - 1 > 2)
        {
            control_.erase(control_.begin() + long(idx));
            coeff_.resize(control_.size());
            detail::bezier_coefficients<T>(control_, coeff_);
            return true;
        }
        return false;
    }
    // Move a control point to a new position
    bool move(size_t idx, const T& new_value)
    {
        if(idx < control_.size())
        {
            control_[idx] = new_value;
            coeff_.resize(control_.size());
            detail::bezier_coefficients<T>(control_, coeff_);
            return true;
        }
        return false;
    }
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
    inline const std::vector<T>& get_control_points() const { return control_; }

    // * Interpolation functions
    // Return the value along the curve at specified parameter value
    inline T value(float tt) const { return detail::bezier_evaluate<0, T>(tt, coeff_); }
    // Return the first derivative at specified parameter value
    inline T prime(float tt) const { return detail::bezier_evaluate<1, T>(tt, coeff_); }
    // Return the second derivative at specified parameter value
    inline T second(float tt) const { return detail::bezier_evaluate<2, T>(tt, coeff_); }

private:
    std::vector<T> control_;
    std::vector<T> coeff_;
};

template <typename T> class HermiteSpline
{
public:
    HermiteSpline(const std::vector<T>& control_points, float tension = 0.f, const T& start_tangent = T(0),
                  const T& end_tangent = T(0))
        : control_(control_points)
    {
        coeff_.resize(control_.size() - 1);

        // Compute tangents (formula for a generic cardinal spline)
        std::vector<T> tangents(control_.size(), T(0));
        tangents[0] = start_tangent;
        for(size_t ii = 1; ii < tangents.size() - 1; ++ii)
            tangents[ii] = (1.f - tension) * (control_[ii + 1] - control_[ii - 1]);
        tangents[control_.size() - 1] = end_tangent;

        // Compute coefficients
        for(size_t ii = 0; ii < control_.size() - 1; ++ii)
        {
            detail::bezier_coefficients<T>({control_[ii], control_[ii] + tangents[ii] / 3.f,
                                            control_[ii + 1] - tangents[ii + 1] / 3.f, control_[ii + 1]},
                                           coeff_[ii]);
        }

        // Compute cumulative arc lengths
        calculate_arc_lengths_iterative(100);
    }

    inline float remap(float tt, size_t idx) const { return tt * float(control_.size() - 1) - float(idx); }

    inline T value(float tt) const
    {
        // Get coefficient index associated to subinterval containing tt
        size_t idx = get_coeff_index(tt);
        return detail::bezier_evaluate<0, T>(remap(tt, idx), coeff_[idx]);
    }

    inline T prime(float tt) const
    {
        size_t idx = get_coeff_index(tt);
        return detail::bezier_evaluate<1, T>(remap(tt, idx), coeff_[idx]);
    }

    inline T second(float tt) const
    {
        size_t idx = get_coeff_index(tt);
        return detail::bezier_evaluate<2, T>(remap(tt, idx), coeff_[idx]);
    }

    // Return arc-length parameterized value
    inline T value_alp(float tt) const { return value(detail::arclen_parameterize<T>(tt, arc_length_)); }
    // Return arc-length parameterized first derivative
    inline T prime_alp(float tt) const { return prime(detail::arclen_parameterize<T>(tt, arc_length_)); }
    // Return arc-length parameterized second derivative
    inline T second_alp(float tt) const { return second(detail::arclen_parameterize<T>(tt, arc_length_)); }

private:
    inline size_t get_coeff_index(float tt) const
    {
        // Clamp index if tt is out of bounds. A value of exactly 1.f would yield
        // an index out of bounds, so the upper bound is set to the number just before 1.f
        tt = std::clamp(tt, 0.f, std::nexttoward(1.f, -1));
        return size_t(std::floor(float(control_.size() - 1) * tt));
    }

    void calculate_arc_lengths_iterative(size_t max_iter)
    {
        arc_length_.resize(max_iter);
        float arclen = 0.f;
        T prev = control_[0];
        for(size_t ii = 0; ii < max_iter; ++ii)
        {
            float tt = float(ii) / float(max_iter - 1);
            T point = value(tt);
            arclen += PointDistance<T>::distance(point, prev);
            arc_length_[ii] = arclen;
            prev = point;
        }
    }

private:
    std::vector<T> control_;
    std::vector<std::array<T, 4>> coeff_;
    std::vector<float> arc_length_;
};

} // namespace math
} // namespace kb