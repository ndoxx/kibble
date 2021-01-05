#pragma once

#include <functional>
#include <limits>
#include <random>

namespace kb
{
namespace opt
{

template <typename ControlT> struct DescentParameters
{
    ControlT initial_control;
    float initial_step = 1.f;
    float initial_epsilon = 0.5f;
    float learning_bias = 0.f;
    float convergence_delta = 0.0005f;
    float alpha = 0.602f;
    float gamma = 0.101f;
    size_t max_iter = 200;
};

template <typename ControlT> struct ControlTraits
{
    using size_type = size_t;
    static constexpr size_type size() { return 0; }
    static void normalize(ControlT&) {}
};

template <typename ControlT, typename RandomEngineT = std::default_random_engine> class StochasticDescentOptimizer
{
public:
    using LossFunc = std::function<float(const ControlT&)>;
    using ConstraintFunc = std::function<void(ControlT&)>;
    using IterCallback = std::function<void(size_t iter, const ControlT&)>;

    StochasticDescentOptimizer(size_t seed) : gen_(seed) {}
    StochasticDescentOptimizer() : gen_(std::random_device()()) {}

    inline void set_loss(LossFunc loss) { loss_ = loss; }
    inline void set_constraint(ConstraintFunc constraint) { constraint_ = constraint; }
    inline void set_iteration_callback(IterCallback iter_callback) { iter_callback_ = iter_callback; }

    ControlT SPSA(const DescentParameters<ControlT>& params)
    {
        std::bernoulli_distribution dis(0.5);

        size_t iter = 0;
        float filtered_loss = 1.f;
        float old_loss = std::numeric_limits<float>::infinity();
        ControlT uu = params.initial_control;
        while(iter < params.max_iter && std::abs(filtered_loss - old_loss) > params.convergence_delta)
        {
            float ak = params.initial_step / std::pow(float(iter + 1) + params.learning_bias, params.alpha);
            float ck = params.initial_epsilon / std::pow(float(iter + 1), params.gamma);

            // Compute forward and backward loss given a random perturbation
            ControlT del;
            for(typename ControlTraits<ControlT>::size_type ii = 0; ii < ControlTraits<ControlT>::size(); ++ii)
                del[ii] = bernoulli_remap(dis(gen_));
            ControlTraits<ControlT>::normalize(del);
            float forward_loss = loss_(uu + ck * del);
            float backward_loss = loss_(uu - ck * del);
            float h = (forward_loss - backward_loss);
            ControlT g_hat;
            for(typename ControlTraits<ControlT>::size_type ii = 0; ii < ControlTraits<ControlT>::size(); ++ii)
                g_hat[ii] = h * (0.5f / (ck * del[ii]));

            // Update and constrain control parameters
            uu -= ak * g_hat;
            constraint_(uu);

            // IIR filter applied to the current loss to limit sensitivity to loss jittering
            float current_loss = 0.5f * (forward_loss + backward_loss);
            old_loss = filtered_loss;
            exponential_moving_average(filtered_loss, current_loss, 0.1f);

            iter_callback_(iter++, uu);
        }
        return uu;
    }

    ControlT FDSA(const DescentParameters<ControlT>& params)
    {
        size_t iter = 0;
        float filtered_loss = 1.f;
        float old_loss = std::numeric_limits<float>::infinity();
        ControlT uu = params.initial_control;
        std::array<ControlT, size_t(ControlTraits<ControlT>::size())> dir;
        for(typename ControlTraits<ControlT>::size_type ii = 0; ii < ControlTraits<ControlT>::size(); ++ii)
        {
            dir[size_t(ii)] = ControlT(0.f);
            dir[size_t(ii)][ii] = 1.f;
        }

        while(iter < params.max_iter && std::abs(filtered_loss - old_loss) > params.convergence_delta)
        {
            float ak = params.initial_step / std::pow(float(iter + 1) + params.learning_bias, params.alpha);
            float ck = params.initial_epsilon / std::pow(float(iter + 1), params.gamma);

            // Compute forward and backward losses for each dimension of the control vector
            ControlT g_hat;
            float current_loss = 0.f;
        	for(typename ControlTraits<ControlT>::size_type ii = 0; ii < ControlTraits<ControlT>::size(); ++ii)
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
            exponential_moving_average(filtered_loss, current_loss, 0.1f);

            iter_callback_(iter++, uu);
        }
        return uu;
    }

private:
    inline float bernoulli_remap(bool value) { return value ? 1.f : -1.f; }

    inline void exponential_moving_average(float& accumulator, float new_value, float alpha)
    {
        accumulator = (alpha * new_value) + (1.f - alpha) * accumulator;
    }

private:
    RandomEngineT gen_;
    LossFunc loss_ = [](const ControlT&) { return 0.f; };
    ConstraintFunc constraint_ = [](ControlT&) {};
    IterCallback iter_callback_ = [](size_t, const ControlT&) {};
};

} // namespace opt
} // namespace kb