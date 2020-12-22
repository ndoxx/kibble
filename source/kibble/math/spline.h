#pragma once

#include <array>
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
constexpr size_t k_max_fac = 10;
constexpr auto k_fac = gen_factorial<k_max_fac>();
} // namespace detail

template <typename T> class BezierSpline
{
public:
    BezierSpline(const std::vector<T> control_points) : control_(control_points) { update_coefficients(); }

    // * Access
    // Add a new control point
    bool add(const T& point)
    {
        if(control_.size() + 1 < detail::k_max_fac)
        {
            control_.push_back(point);
            update_coefficients();
            return true;
        }
        return false;
    }
    bool insert(size_t idx, const T& point)
    {
        if(idx < control_.size() && control_.size() + 1 < detail::k_max_fac)
        {
            control_.insert(control_.begin() + long(idx), point);
            update_coefficients();
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
            update_coefficients();
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
            update_coefficients();
            return true;
        }
        return false;
    }
    // Get a control point
    inline const T& get_control_point(size_t idx)
    {
        K_ASSERT(idx < control_.size(), "Index out of bounds.");
        return control_.at(idx);
    }
    // Get first control point
    inline const T& front() { return control_.front(); }
    // Get last control point
    inline const T& back() { return control_.back(); }
    // Get all control points
    inline const std::vector<T>& get_control_points() const { return control_; }

    // * Interpolation functions
    // Return the value along the curve at specified parameter value
    T value(float tt)
    {
        T sum(0.f);
        float tpow = 1.f;
        for(size_t ii = 0; ii < coeff_.size(); ++ii)
        {
            sum += tpow * coeff_[ii];
            tpow *= tt;
        }
        return sum;
    }
    // Return the first derivative at specified parameter value
    T prime(float tt)
    {
        T sum(0.f);
        float tpow = 1.f;
        for(size_t ii = 0; ii < coeff_.size() - 1; ++ii)
        {
            sum += tpow * float(ii + 1) * coeff_[size_t(ii + 1)];
            tpow *= tt;
        }
        return sum;
    }
    // Return the second derivative at specified parameter value
    T second(float tt)
    {
        T sum(0.f);
        float tpow = 1.f;
        for(size_t ii = 0; ii < coeff_.size() - 2; ++ii)
        {
            sum += tpow * float((ii + 1) * (ii + 2)) * coeff_[size_t(ii + 2)];
            tpow *= tt;
        }
        return sum;
    }
    // Shortcut for value interpolation
    inline T operator()(float tt) { return value(tt); }
    // Stateless interpolation (deCasteljau)
    static inline T interpolate_deCasteljau(float tt, const std::vector<T>& points)
    {
        return deCasteljau(points.size() - 1, 0, tt, points);
    }

private:
    void update_coefficients()
    {
        K_ASSERT(control_.size() > 2, "There must be at least 3 control points.");
        K_ASSERT(control_.size() < detail::k_max_fac, "Maximum number of control points exceeded.");

        coeff_.resize(control_.size());

        const int nn = int(control_.size());
        int prod = 1;
        for(int jj = 0; jj < nn; ++jj)
        {
            // Product from m=0 to j-1 of (n-m)
            prod *= (jj > 0) ? nn - jj : 1;

            T sum(0.f);
            for(int ii = 0; ii <= jj; ++ii)
            {
                int comb = (prod * parity(ii + jj)) / (detail::k_fac[size_t(ii)] * detail::k_fac[size_t(jj - ii)]);
                sum += float(comb) * control_[size_t(ii)];
            }
            coeff_[size_t(jj)] = sum;
        }
    }

    // Stateless interpolation using deCasteljau's algorithm
    // Parameters: recursion level, free index, interpolation parameter and control points
    static T interpolate_deCasteljau(unsigned rr, unsigned ii, float tt, const std::vector<T>& points)
    {
        if(rr == 0)
            return points[ii];

        T p1 = deCasteljau(rr - 1, ii, tt, points);
        T p2 = deCasteljau(rr - 1, ii + 1, tt, points);

        return p1 * (1.f - tt) + p2 * tt;
    }

private:
    std::vector<T> control_;
    std::vector<T> coeff_;
};

} // namespace math
} // namespace kb