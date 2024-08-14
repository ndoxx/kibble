#include "kibble/algorithm/optimization.h"
#include "../tests/common/vec.h"

#include <iostream>

using namespace kb;

// We need to define how the algorithm should interact with our vec2
namespace kb::opt
{
template <>
struct ControlTraits<vec2>
{
    using size_type = size_t;
    static constexpr size_type size()
    {
        return 2;
    }
    static void normalize(vec2& vec)
    {
        vec.normalize();
    }
};
} // namespace kb::opt

struct OptimizationProblem
{
    std::string name;
    kb::opt::DescentParameters<vec2> params;
    std::function<float(const vec2&)> loss;
    vec2 expected_control;
};

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /*
     * In this example, we will use the stochastic descent optimizer to solve a series of minimization problems.
     * The data for each problem is saved in a struct and pushed to a vector, then we will solve each problem
     * by iterating this vector. That may not be the most lisible way to do it, but it avoids duplicating code.
     */
    std::vector<OptimizationProblem> problems;

    /*
     * Let's minimize the bivariate convex function x^2+xy+y^2, with an initial guess of (1,1)
     * using the SPSA algorithm.
     * We know from a simple calculation that the minimum is located at (0,0), so the optimal
     * control vector should approach this value.
     */
    problems.push_back({"(convex) oblate paraboloid function",
                        {vec2(1.f, 1.f), 1.f, 0.5f, 0.f, 0.0005f},
                        [](const vec2& u) {
                            // J(x,y) = x^2 + xy + y^2
                            return u.x() * u.x() + u.x() * u.y() + u.y() * u.y();
                        },
                        vec2(0.f, 0.f)});

    /*
     * Next is the non-convex Himmelblau's function.
     * This function has 4 identical local minima, one of which is located at (3.584428,-1.848126)
     * By starting at (5,-2) we are targeting this local minimum
     * https://en.wikipedia.org/wiki/Himmelblau%27s_function
     */
    problems.push_back({"(non-convex) Himmelblau's function",
                        {vec2(5.f, -2.f), 0.01f, 0.005f, 0.f, 1e-3f},
                        [](const vec2& u) {
                            // J(x,y) = (x^2+y-11)^2 + (x+y^2-7)^2
                            float A = (u.x() * u.x() + u.y() - 11);
                            float B = (u.x() + u.y() * u.y() - 7);
                            return A * A + B * B;
                        },
                        vec2(3.584428f, -1.848126f)});

    /*
     * Then we try to minimize the Rosenbrock function. The global minimum of this function is hard to converge to.
     * In fact, the convergence in this example will be a bit off, I'm sure I can get close to the expected minimum
     * by fiddling with the parameters, but I'm too lazy.
     * https://en.wikipedia.org/wiki/Rosenbrock_function
     */
    problems.push_back({"(non-convex) Rosenbrock's function",
                        {vec2(1.5f, 1.5f), 0.001f, 0.0005f, 0.f, 1e-4f},
                        [](const vec2& u) {
                            // J(x,y) = (a-x)^2 + b(y-x^2)^2
                            constexpr float a = 1.f;
                            constexpr float b = 100.f;
                            return (a - u.x()) * (a - u.x()) + b * (u.y() - u.x() * u.x()) * (u.y() - u.x() * u.x());
                        },
                        vec2(1.f, 1.f)});

    // Setup an optimizer with 42 as a seed
    opt::StochasticDescentOptimizer<vec2> optimizer(42);

    // Every 5 iterations, print the state
    optimizer.set_iteration_callback([](size_t iter, const vec2& control, float filtered_loss) {
        if (iter % 10 == 0)
        {
            std::cout << "Iteration #" << iter << ": control=" << control << " mean loss=" << filtered_loss
                      << std::endl;
        }
    });

    // Solve each problem
    for (const auto& problem : problems)
    {
        std::cout << "Minimizing the " << problem.name << " starting at " << problem.params.initial_control << '.'
                  << std::endl;

        // Set the loss function
        optimizer.set_loss(problem.loss);

        // Perform the descent and get an estimation for the optimal control vector
        auto optimal = optimizer.SPSA(problem.params);

        // Compute the difference with the expected optimal control
        auto dist = problem.expected_control - optimal;

        std::cout << "Optimal control point is: " << optimal << std::endl;
        std::cout << "This should be close to " << problem.expected_control << " (deviation= " << dist.norm() << ')'
                  << std::endl;
    }

    return 0;
}