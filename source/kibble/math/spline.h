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

template <size_t DIFF_ORDER, typename T, typename VecT> static T bezier_evaluate(float tt, const VecT& coeffs)
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
template <typename T, typename VecT> static T deCasteljau(unsigned rr, unsigned ii, float tt, const VecT& points)
{
    if(rr == 0)
        return points[ii];

    T p1 = deCasteljau(rr - 1, ii, tt, points);
    T p2 = deCasteljau(rr - 1, ii + 1, tt, points);

    return p1 * (1.f - tt) + p2 * tt;
}
} // namespace detail

// Stateless interpolation (deCasteljau)
template <typename T> static inline T deCasteljau(float tt, const std::vector<T>& points)
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

private:
    size_t get_coeff_index(float tt) const
    {
        // Clamp index if tt is out of bounds
        if(tt < 0.f)
            return 0;
        if(tt >= 1.f)
            return coeff_.size() - 1;
        // Find immediate upper bound in domain and return index of lower bound
        for(size_t ii = 1; ii < control_.size(); ++ii)
        {
            if(tt < float(ii) / float(control_.size() - 1))
                return ii - 1;
        }
        return 0;
    }

private:
    std::vector<T> control_;
    std::vector<std::array<T, 4>> coeff_;
};

} // namespace math
} // namespace kb