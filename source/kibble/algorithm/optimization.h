#pragma once

#include <functional>
#include <limits>
#include <random>

#include "math/numeric.h"

namespace kb
{
namespace opt
{

/**
 * @brief Structure containing parameters guiding the descent algorithm.
 *
 * @tparam ControlT Type representing a vector of values to optimize
 * @note For a comprehensive overview of the gain sequences scheduling and the
 * recommended values to use here, see:\n
 * Spall, IEEE Transactions on Aerospace and Electronics Systems, 1998, pp817-823\n
 * https://www.jhuapl.edu/spsa/PDF-SPSA/Spall_Implementation_of_the_Simultaneous.PDF
 */
template <typename ControlT>
struct DescentParameters
{
    /// Initial values used as a starting point in the optimization process
    ControlT initial_control;
    /// Initial value @f$a@f$ of the learning rate (gain sequence @f$a_n@f$)
    float initial_step = 1.f;
    /// Initial value @f$c@f$ of the perturbation radius (gain sequence @f$c_n@f$)
    float initial_radius = 0.5f;
    /// Bias term @f$A@f$ in the denominator of the power law for @f$a_n@f$
    float learning_bias = 0.f;
    /// Loss function difference convergence criterion @f$\delta@f$
    float convergence_delta = 0.0005f;
    /// Power law exponent for the @f$a_n@f$ schedule
    float alpha = 0.602f;
    /// Power law exponent for the @f$c_n@f$ schedule
    float gamma = 0.101f;
    /// Maximum number of iterations
    size_t max_iter = 200;
};

/**
 * @brief Describe how the type ControlT should be used by the algorithm.
 *
 * The descent algorithm will need to access the size of the control vector, meaning
 * the number of components to optimize, and will also need a way to normalize it.
 * Specialize this struct to make the optimizer able to use any custom type.
 *
 * @tparam ControlT
 */
template <typename ControlT>
struct ControlTraits
{
    /**
     * @brief Return type of the size() function
     */
    using size_type = size_t;

    /**
     * @brief Return the number of components in a ControlT
     * @return constexpr size_type
     */
    static constexpr size_type size()
    {
        return 0;
    }

    /**
     * @brief Normalize a ControlT vector
     */
    static void normalize(ControlT&)
    {
    }
};

/**
 * @brief This class implements the FDSA and SPSA descent algorithms.
 *
 * The goal of this algorithm is to estimate an optimal value for a set of parameters
 * grouped in a *control vector*, such that a given function called the *loss* is minimal
 * at that point.\n
 * The general idea is to iteratively compute an estimation for the gradient of the
 * loss function, and follow it till the loss difference from one iteration to the
 * next stabilizes.
 * Two approximations for the gradient can be used, giving rise to two algorithms.\n
 * Let @f$u@f$ be a control vector and @f$\mathcal{J}(u)@f$ the cost at @f$u@f$. Also denote
 * by @f$u_n@f$ the n-th iteration estimation of the optimal control point. Let @f$c_n@f$ be
 * a small positive number such that @f$\lim_{n \to +\infty} c_n = 0@f$, @f$e_i@f$ the i-th basis vector and
 * @f$\Delta_n@f$ a vector of independant random Bernoulli variables @f$\Delta_n^i@f$. Then:\n
 * The **Finite Difference Stochastic Approximation** (FDSA) gradient estimator is:
 * @f[ \hat{g}_n^i(u_n) = \frac{\mathcal{J}(u_n + c_n e_i) - \mathcal{J}(u_n - c_n e_i)}{2 c_n} @f]
 * The **Simultaneous Perturbation Stochastic Approximation** (SPSA) gradient estimator is:
 * @f[ \hat{g}_n^i(u_n) = \frac{\mathcal{J}(u_n + c_n \Delta_n) - \mathcal{J}(u_n - c_n \Delta_n)}{2 c_n
 * \Delta_n^i} @f]
 * Note that the SPSA estimator requires less evaluations of the loss function. This is very important to keep in mind
 * when the evaluation takes time, which is usually the case in hard optimization problems. SPSA will always be faster.
 *
 * At each iteration, a gradient estimator is computed, then the control point is updated based on the estimator:
 * @f[ u_{n+1} = u_n - a_n \hat{g}_n(u_n)  @f]
 * With the *learning rate* @f$a_n@f$ a small positive number such that @f$\lim_{n \to +\infty} a_n = 0@f$\n
 *
 * The mean loss @f$\bar{\mathcal{J}}_n@f$ is computed as the mean of the forward and backward losses, and is
 * then filtered thanks to an exponential moving average IIR so as to avoid early convergence due to loss jittering:
 * @f[ \langle \mathcal{J}_0 \rangle = \bar{\mathcal{J}}_0 @f]
 * @f[ \langle \mathcal{J}_n \rangle = \beta \bar{\mathcal{J}}_n + (1-\beta) \langle \mathcal{J}_{n-1} \rangle @f]
 * with @f$\beta = 0.1@f$.
 * The algorithm stops when the absolute difference of two consecutive filtered losses become smaller than
 * the convergence criterion: @f$ |\langle \mathcal{J}_n \rangle - \langle \mathcal{J}_{n-1} \rangle| < \delta @f$
 *
 * It is of critical importance for the algorithm stability that the *gain sequences* @f$a_n@f$ and @f$c_n@f$
 * follow a power law:
 * @f[ a_n = \frac{a}{(n+1+A)^{\alpha}}  @f]
 * @f[ c_n = \frac{c}{(n+1)^{\gamma}}  @f]
 * The exact shape of the power law is controlled by the input DescentParameters struct.
 *
 * @tparam ControlT
 * @tparam RandomEngineT
 */
template <typename ControlT, typename RandomEngineT = std::default_random_engine>
class StochasticDescentOptimizer
{
public:
    using LossFunc = std::function<float(const ControlT&)>;
    using ConstraintFunc = std::function<void(ControlT&)>;
    using IterCallback = std::function<void(size_t iter, const ControlT&, float)>;

    /**
     * @brief Construct a new Stochastic Descent Optimizer and initialize its random number generator with a seed.
     *
     * @param seed The RNG seed. Two SPSA calls with the same seed always return the same result.
     */
    StochasticDescentOptimizer(size_t seed) : gen_(seed)
    {
    }

    /**
     * @brief Construct a new Stochastic Descent Optimizer and initialize its random number generator with a random
     * seed.
     */
    StochasticDescentOptimizer() : gen_(std::random_device()())
    {
    }

    /**
     * @brief Set the loss function functor
     *
     * @param loss The loss function takes a control vector and returns a cost as a float number
     */
    inline void set_loss(LossFunc loss)
    {
        loss_ = loss;
    }

    /**
     * @brief Set the constraint function
     *
     * This function will mutate the control vector after each update. It allows to constrain the parameter space.
     *
     * @param constraint The constraint function takes a reference to a control vector and mutates it
     */
    inline void set_constraint(ConstraintFunc constraint)
    {
        constraint_ = constraint;
    }

    /**
     * @brief Set the iteration callback function
     *
     * This function will be called after each iteration. This allows to track the algorithm progress,
     * which is often desirable when it takes time to complete.
     *
     * @param iter_callback
     */
    inline void set_iteration_callback(IterCallback iter_callback)
    {
        iter_callback_ = iter_callback;
    }

    /**
     * @brief Perform a stochastic descent using an SPSA approximator.
     *
     * @param params Structure containing all the parameters guiding the algorithm
     * @return ControlT
     */
    ControlT SPSA(const DescentParameters<ControlT>& params)
    {
        std::bernoulli_distribution dis(0.5);

        size_t iter = 0;
        float filtered_loss = 1.f;
        float old_loss = std::numeric_limits<float>::infinity();
        ControlT uu = params.initial_control;
        while (iter < params.max_iter && std::abs(filtered_loss - old_loss) > params.convergence_delta)
        {
            float ak = params.initial_step / std::pow(float(iter + 1) + params.learning_bias, params.alpha);
            float ck = params.initial_radius / std::pow(float(iter + 1), params.gamma);

            // Compute forward and backward loss given a random perturbation
            ControlT del;
            using size_type = typename ControlTraits<ControlT>::size_type;
            constexpr auto sz = ControlTraits<ControlT>::size();
            for (size_type ii = 0; ii < sz; ++ii)
                del[ii] = bernoulli_remap(dis(gen_));
            ControlTraits<ControlT>::normalize(del);
            float forward_loss = loss_(uu + ck * del);
            float backward_loss = loss_(uu - ck * del);
            float h = (forward_loss - backward_loss);
            ControlT g_hat;
            for (size_type ii = 0; ii < sz; ++ii)
                g_hat[ii] = h * (0.5f / (ck * del[ii]));

            // Update and constrain control parameters
            uu -= ak * g_hat;
            constraint_(uu);

            // IIR filter applied to the current loss to limit sensitivity to loss jittering
            float current_loss = 0.5f * (forward_loss + backward_loss);
            old_loss = filtered_loss;
            math::exponential_moving_average(filtered_loss, current_loss, 0.1f);

            iter_callback_(iter++, uu, filtered_loss);
        }
        return uu;
    }

    /**
     * @brief Perform a stochastic descent using an FDSA approximator.
     *
     * @param params Structure containing all the parameters guiding the algorithm
     * @return ControlT
     */
    ControlT FDSA(const DescentParameters<ControlT>& params)
    {
        size_t iter = 0;
        float filtered_loss = 1.f;
        float old_loss = std::numeric_limits<float>::infinity();
        ControlT uu = params.initial_control;
        using size_type = typename ControlTraits<ControlT>::size_type;
        constexpr auto sz = ControlTraits<ControlT>::size();
        std::array<ControlT, size_t(sz)> dir;
        for (size_type ii = 0; ii < sz; ++ii)
        {
            dir[size_t(ii)] = ControlT(0.f);
            dir[size_t(ii)][ii] = 1.f;
        }

        while (iter < params.max_iter && std::abs(filtered_loss - old_loss) > params.convergence_delta)
        {
            float ak = params.initial_step / std::pow(float(iter + 1) + params.learning_bias, params.alpha);
            float ck = params.initial_radius / std::pow(float(iter + 1), params.gamma);

            // Compute forward and backward losses for each dimension of the control vector
            ControlT g_hat;
            float current_loss = 0.f;
            for (size_type ii = 0; ii < sz; ++ii)
            {
                float forward_loss = loss_(uu + ck * dir[size_t(ii)]);
                float backward_loss = loss_(uu - ck * dir[size_t(ii)]);
                current_loss += forward_loss + backward_loss;
                g_hat[ii] = (0.5f / ck) * (forward_loss - backward_loss);
            }
            current_loss /= float(2 * dir.size());

            // Update and constrain control parameters
            uu -= ak * g_hat;
            constraint_(uu);

            // IIR filter applied to the current loss to limit sensitivity to loss jittering
            old_loss = filtered_loss;
            math::exponential_moving_average(filtered_loss, current_loss, 0.1f);

            iter_callback_(iter++, uu, filtered_loss);
        }
        return uu;
    }

private:
    // Remap a Bernoulli variable between -1.f and 1.f
    inline float bernoulli_remap(bool value)
    {
        return value ? 1.f : -1.f;
    }

private:
    RandomEngineT gen_;
    LossFunc loss_ = [](const ControlT&) { return 0.f; };
    ConstraintFunc constraint_ = [](ControlT&) {};
    IterCallback iter_callback_ = [](size_t, const ControlT&, float) {};
};

} // namespace opt
} // namespace kb