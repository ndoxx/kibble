#pragma once
#include <cmath>

namespace kb
{
namespace math
{

/**
 * @brief A catenary is the natural shape taken by a thin massive cable hanging between two points refered to as
 * anchors. It is essentially an hyperbolic cosine. Solving for the catenary parameters requires solving a
 * transcendental equation, which can be done approximately with a few iterations of the Newton-Raphson algorithm.
 * Expect instability for very distant anchor points. It is always a good strategy to normalize the input data and
 * rescale the curve. This class also uses a closed-form arc-length parameterization of the catenary, allowing for a
 * uniform percent length sampling along the curve.
 *
 */
class Catenary
{
public:
    /**
     * @brief Construct a catenary curve.
     * The curve of length s will hang between anchor points (x1,y1) and (x2,y2).
     *
     * @param x1 x coordinate of the first anchor
     * @param y1 y coordinate of the first anchor
     * @param x2 x coordinate of the second anchor
     * @param y2 y coordinate of the second anchor
     * @param s desired length of the curve
     * @param max_error the maximal error for parameter estimation
     */
    Catenary(float x1, float y1, float x2, float y2, float s, float max_error = 0.001f);

    /**
     * @brief Return the y value given the x value
     *
     * @param xx
     * @return float
     */
    inline float value(float xx) const
    {
        return a_ * std::cosh(((reflect_ ? -xx + m_ : xx) - p_) / a_) + q_;
    }

    /**
     * @brief Return the first derivative at specified x value
     *
     * @param xx
     * @return float
     */
    inline float prime(float xx) const
    {
        return std::sinh(((reflect_ ? -xx + m_ : xx) - p_) / a_);
    }

    /**
     * @brief Return arc-length parameterized value
     *
     * @param ss The arc length fraction, between 0 and 1
     * @return float
     */
    inline float value_arclen(float ss) const
    {
        return value(arclen_remap(ss));
    }

    /**
     * @brief Return arc-length parameterized first derivative
     *
     * @param ss The arc length fraction, between 0 and 1
     * @return float
     */
    inline float prime_arclen(float ss) const
    {
        return prime(arclen_remap(ss));
    }

    /**
     * @brief Return the value of x for the specified length fraction ss
     *
     * @param ss The arc length fraction, between 0 and 1
     * @return float
     */
    float arclen_remap(float ss) const;

private:
    /// Scale parameter
    float a_;              
    /// x-offset
    float p_;              
    /// y-offset
    float q_;              
    /// Anchors midpoint
    float m_;              
    /// Integration constant for arc-length parameterization
    float C_;              
    /// Full length between anchor points
    float s_;              
    /// Reflect curve w.r.t midpoint?
    bool reflect_ = false; 
};

} // namespace math
} // namespace kb