#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <ostream>

namespace kb
{
namespace math
{

/**
 * @brief Lightweight class that allows to keep track of various basic statistics on a series of values pushed one by
 * one. The mean and standard deviation are updated incrementally in an online fashion thanks to an orthogonality trick,
 * so the whole sequence of numbers need not be memorized. This guarantees constant time computation of all statistics.
 * Inspired by https://github.com/vectorgraphics/asymptote/blob/master/statistics.h
 */
template <typename FloatT = float>
requires std::floating_point<FloatT>
class Statistics
{
public:
    /**
     * @brief Push a value and update statistics.
     *
     * @param val New value to take into account
     */
    void push(FloatT val)
    {
        FloatT diff = val - mean_;
        mean_ += diff / ++count_;
        FloatT var = diff * (val - mean_);
        var_[size_t(diff >= FloatT(0))] += var;
    }

    /**
     * @brief Reset the statistics.
     *
     */
    void reset()
    {
        count_ = {0};
        mean_ = {0};
        var_[0] = {0};
        var_[1] = {0};
    }

    /**
     * @brief Push all values within an iterator range.
     *
     * @tparam InputIt
     * @param first Iterator to the first value
     * @param last Iterator past the last value
     */
    template <class InputIt>
    inline void run(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](FloatT val) { push(val); });
    }

    /**
     * @brief Get the number of values pushed.
     *
     * @return std::size_t
     */
    inline std::size_t count() const
    {
        return std::size_t(count_);
    }

    /**
     * @brief Get the mean
     *
     * @return FloatT
     */
    inline FloatT mean() const
    {
        return mean_;
    }

    /**
     * @brief Get the standard deviation
     *
     * @return FloatT
     */
    inline FloatT stdev() const
    {
        return stdev(var_[0] + var_[1], {1});
    }

    /**
     * @brief Get the lower deviation (typical deviation under the mean)
     *
     * @return FloatT
     */
    inline FloatT stdev_l() const
    {
        return stdev(var_[0], {2});
    }

    /**
     * @brief Get the upper deviation (typical deviation over the mean)
     *
     * @return FloatT
     */
    inline FloatT stdev_u() const
    {
        return stdev(var_[1], {2});
    }

    /**
     * @brief Serialize all statistics to a stream
     *
     * @param stream
     * @param stats
     * @return std::ostream&
     */
    friend std::ostream& operator<<(std::ostream& stream, const Statistics& stats)
    {
        stream << stats.mean() << " [\u00b1" << stats.stdev() << "] (+" << stats.stdev_l() << "/-" << stats.stdev_u()
               << ')';
        return stream;
    }

private:
    FloatT stdev(FloatT var, FloatT f) const
    {
        FloatT factor = count_ > f ? f / (count_ - f) : FloatT(0);
        return std::sqrt(var * factor);
    }

private:
    FloatT count_ = {0};
    FloatT mean_ = {0};
    FloatT var_[2] = {{0}, {0}};
};

} // namespace math
} // namespace kb