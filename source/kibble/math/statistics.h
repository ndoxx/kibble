#pragma once

#include <algorithm>
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
class Statistics
{
public:
    /**
     * @brief Push a value and update statistics.
     *
     * @param val New value to take into account
     */
    void push(float val);

    /**
     * @brief Reset the statistics.
     *
     */
    void reset();

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
        std::for_each(first, last, [this](float val) { push(val); });
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
     * @return float
     */
    inline float mean() const
    {
        return mean_;
    }

    /**
     * @brief Get the standard deviation
     *
     * @return float
     */
    inline float stdev() const
    {
        return stdev(var_[0] + var_[1], 1.f);
    }

    /**
     * @brief Get the lower deviation (typical deviation under the mean)
     *
     * @return float
     */
    inline float stdev_l() const
    {
        return stdev(var_[0], 2.f);
    }

    /**
     * @brief Get the upper deviation (typical deviation over the mean)
     *
     * @return float
     */
    inline float stdev_u() const
    {
        return stdev(var_[1], 2.f);
    }

    /**
     * @brief Serialize all statistics to a stream
     *
     * @param stream
     * @param stats
     * @return std::ostream&
     */
    friend std::ostream &operator<<(std::ostream &stream, const Statistics &stats);

private:
    float stdev(float var, float f) const;

private:
    float count_ = 0;
    float mean_ = 0;
    float var_[2] = {0, 0};
};

} // namespace math
} // namespace kb