#include "kibble/math/convolution.h"
#include "kibble/assert/assert.h"
#include "kibble/math/numeric.h"

#include <cmath>
#include <functional>

namespace kb
{
namespace math
{

constexpr uint32_t k_simpson_subdivisions = 6;

static float gaussian_distribution(float x, float mu, float sigma)
{
    float d = x - mu;
    float n = 1.0f / (std::sqrt(2.0f * float(M_PI)) * sigma);
    return std::exp(-d * d / (2 * sigma * sigma)) * n;
};

SeparableGaussianKernel::SeparableGaussianKernel(uint32_t size, float sigma)
{
    init(size, sigma);
}

void SeparableGaussianKernel::init(uint32_t size, float _sigma)
{
    K_ASSERT(size % 2 == 1, "Gaussian kernel size must be odd. Got: {}", size);
    K_ASSERT((size + 1) / 2 <= k_max_kernel_coefficients, "Gaussian kernel size out of bounds: {}", size);
    K_ASSERT(_sigma > 0.f, "Gaussian kernel sigma must be strictly positive. sigma={}", _sigma);

    half_size = (size + 1) / 2;

    std::fill(weights, weights + k_max_kernel_coefficients, 0.f);

    // Compute weights by numerical integration of distribution over each kernel tap
    float sum = 0.0f;
    for (uint32_t ii = 0; ii < half_size; ++ii)
    {
        float x_lb = float(ii) - 0.5f;
        float x_ub = float(ii) + 0.5f;
        weights[ii] = integrate_simpson([&](float x) { return gaussian_distribution(x, 0.0f, _sigma); }, x_lb, x_ub,
                                        k_simpson_subdivisions);
        // First (central) weight is counted once, other elements must be counted twice
        sum += ((ii == 0) ? 1.0f : 2.0f) * weights[ii];
    }

    // Renormalize weights to unit
    for (uint32_t ii = 0; ii < half_size; ++ii)
    {
        weights[ii] /= sum;
    }
}

std::vector<float> SeparableGaussianKernel::unfold() const
{
    // Total kernel size
    std::vector<float> full_kernel(2 * half_size + 1);
    // Central coefficient (at the middle of the array)
    full_kernel[half_size] = weights[0];

    // Unfold symmetric coefficients
    for (uint32_t ii = 1; ii <= half_size; ++ii)
    {
        full_kernel[half_size - ii] = weights[ii];
        full_kernel[half_size + ii] = weights[ii];
    }

    return full_kernel;
}

std::vector<float> convolve_1D(const std::vector<float>& input, const std::vector<float>& kernel)
{
    // Sanity check
    K_ASSERT(!input.empty(), "Input cannot be empty");
    K_ASSERT(!kernel.empty(), "Kernel cannot be empty");
    K_ASSERT(kernel.size() % 2 == 1, "Kernel must be of odd size");

    // Output vector will have the same size as input
    std::vector<float> output(input.size());
    int32_t half_size = int32_t(kernel.size() / 2);

    // Iterate through each output sample
    for (size_t ii = 0; ii < input.size(); ++ii)
    {
        float sum = 0.0f;

        // Apply kernel around current sample
        for (int32_t kk = -half_size; kk <= half_size; ++kk)
        {
            int32_t in_idx = static_cast<int32_t>(ii) + kk;
            int32_t ker_idx = kk + half_size;

            // Mirror at boundaries
            in_idx = std::abs(in_idx);

            if (in_idx >= int32_t(input.size()))
            {
                in_idx = 2 * int32_t(input.size() - 1) - in_idx;
            }

            sum += input[size_t(in_idx)] * kernel[size_t(ker_idx)];
        }

        output[ii] = sum;
    }

    return output;
}

} // namespace math
} // namespace kb