#pragma once

#include <algorithm>
#include <cstdint>
#include <ostream>

namespace kb
{
namespace math
{

// Inspired by:
// https://github.com/vectorgraphics/asymptote/blob/master/statistics.h
class Statistics
{
public:
    void push(float val);
    void reset();
    template <class InputIt> inline void run(InputIt first, InputIt last)
    {
        std::for_each(first, last, [this](float val) { push(val); });
    }

    inline std::size_t count() const { return std::size_t(count_); }
    inline float mean() const { return mean_; }
    inline float stdev() const { return stdev(var_[0] + var_[1], 1.f); }
    inline float stdev_l() const { return stdev(var_[0], 2.f); }
    inline float stdev_u() const { return stdev(var_[1], 2.f); }

    friend std::ostream& operator<<(std::ostream& stream, const Statistics& stats);

private:
    float stdev(float var, float f) const;

private:
    float count_ = 0;
    float mean_ = 0;
    float var_[2] = {0, 0};
};

} // namespace math
} // namespace kb