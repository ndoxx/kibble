#pragma once

#include <cstdint>
#include <ostream>

namespace kb
{
namespace math
{

static constexpr uint32_t k_max_kernel_coefficients = 12;

/**
 * @brief This structure handles the initialization of arbitrary separable Gaussian convolution kernels.
 * Given that a Gaussian kernel is symmetric wrt the central element, only half of the coefficients need be stored. A
 * separable kernel with a half-size of N+1, will correspond to a convolution matrix of size (2*N+1) x (2*N+1). Example:
 * half-size = 3 -> convolution matrix is 5x5. Weights are normalized, no need to renormalize to unity after a
 * convolution.
 *
 */
struct SeparableGaussianKernel
{
    SeparableGaussianKernel() = default;

    /**
     * @brief Construct a new Separable Gaussian Kernel from a full size and a standard deviation.
     *
     * @param size Odd number less than 23
     * @param sigma Strictly positive standard deviation
     */
    SeparableGaussianKernel(uint32_t size, float sigma = 1.f);

    /**
     * @brief Initialize this kernel from a full size and a standard deviation.
     *
     * @param size Odd number less than 23
     * @param sigma Strictly positive standard deviation
     */
    void init(uint32_t size, float sigma);

    /**
     * @brief Serialize the kernel coefficients to a stream.
     *
     * @param stream
     * @param gk
     * @return std::ostream&
     */
    friend std::ostream& operator<<(std::ostream& stream, const SeparableGaussianKernel& gk);

    float weights[k_max_kernel_coefficients]; /// First half of the kernel weights, including central element
    uint32_t half_size;                       /// Half size of the kernel
};

} // namespace math
} // namespace kb