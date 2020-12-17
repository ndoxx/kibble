#pragma once

#include <cstdint>
#include <ostream>

namespace kb
{
namespace math
{

static constexpr uint32_t k_max_kernel_coefficients = 12;

/*
	This structure handles the initialization of arbitrary separable Gaussian convolution kernels.
	Given that a Gaussian kernel is symmetric wrt the central element, only half of the coefficients
	need be stored. This is why a half-size is provided as a ctor and init argument. A separable
	kernel with a half-size of N+1, will correspond to a convolution matrix of size 
	(2*N+1) x (2*N+1). Example: half-size = 3 -> convolution matrix is 5x5.
	Weights are normalized, no need to renormalize to unity after a convolution.
*/
struct SeparableGaussianKernel
{
	SeparableGaussianKernel() = default;
	SeparableGaussianKernel(uint32_t size, float sigma=1.f);

	void init(uint32_t size, float sigma);

    friend std::ostream& operator<<(std::ostream& stream, const SeparableGaussianKernel& gk);

	float weights[k_max_kernel_coefficients];
	uint32_t half_size;
};

} // namespace math
} // namespace kb