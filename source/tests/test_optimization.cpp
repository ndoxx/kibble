#include "algorithm/optimization.h"
#include "common/vec.h"
#include <catch2/catch_all.hpp>

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
    static void normalize(vec2 &vec)
    {
        vec.normalize();
    }
};
} // namespace kb::opt

class MinimizationFixture
{
public:
    MinimizationFixture() : optimizer(42)
    {
    }

protected:
    kb::opt::StochasticDescentOptimizer<vec2> optimizer;
};

TEST_CASE_METHOD(MinimizationFixture, "Convex function minimization", "[opt]")
{
    optimizer.set_loss([](const vec2 &u) {
        // J(x,y) = x^2 + xy + y^2
        return u.x() * u.x() + u.x() * u.y() + u.y() * u.y();
    });
    auto opt = optimizer.SPSA({vec2(1.f, 1.f), 1.f, 0.5f, 0.f, 0.0005f});
    auto dist = vec2(0.f, 0.f) - opt;

    REQUIRE(dist.norm() < 1e-8f);
}

TEST_CASE_METHOD(MinimizationFixture, "Non-convex Himmelblau's function minimization", "[opt]")
{
    optimizer.set_loss([](const vec2 &u) {
        // J(x,y) = (x^2+y-11)^2 + (x+y^2-7)^2
        float A = (u.x() * u.x() + u.y() - 11);
        float B = (u.x() + u.y() * u.y() - 7);
        return A * A + B * B;
    });
    auto opt = optimizer.SPSA({vec2(5.f, -2.f), 0.001f, 0.0005f, 0.f, 1e-3f});
    auto dist = vec2(3.584428f, -1.848126f) - opt;

    REQUIRE(dist.norm() < 0.2f);
}