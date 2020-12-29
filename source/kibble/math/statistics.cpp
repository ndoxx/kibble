#include "math/statistics.h"
#include <cmath>
#include <iomanip>

namespace kb
{
namespace math
{

void Statistics::push(float val)
{
    float diff = val - mean_;
    mean_ += diff / ++count_;
    float var = diff * (val - mean_);
    var_[size_t(diff >= 0.f)] += var;
}

void Statistics::reset()
{
    count_ = 0;
    mean_ = 0;
    var_[0] = 0;
    var_[1] = 0;
}

float Statistics::stdev(float var, float f) const
{
    float factor = count_ > f ? f / (count_ - f) : 0.f;
    return std::sqrt(var * factor);
}

std::ostream& operator<<(std::ostream& stream, const Statistics& stats)
{
    stream << stats.mean() << " [\u00b1" << stats.stdev() << "] (+" << stats.stdev_l() << "/-" << stats.stdev_u() << ')';
    return stream;
}

} // namespace math
} // namespace kb