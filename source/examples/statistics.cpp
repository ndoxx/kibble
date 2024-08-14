#include "kibble/math/statistics.h"

#include <cmath>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

using namespace kb;

/**
 * @brief Serialize all statistics to a stream
 *
 * @param stream
 * @param stats
 * @return std::ostream&
 */
template <typename T>
std::ostream& operator<<(std::ostream& stream, const math::Statistics<T>& stats)
{
    stream << stats.mean() << " [\u00b1" << stats.stdev() << "] (+" << stats.stdev_l() << "/-" << stats.stdev_u()
           << ')';
    return stream;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    {
        math::Statistics<> stats;

        std::cout << "Adding integers from 1 to 4" << std::endl;
        for (size_t ii = 1; ii < 5; ++ii)
        {
            stats.push(float(ii));
            std::cout << stats << std::endl;
        }
        std::cout << "Adding integers from 4 to 1" << std::endl;
        for (size_t ii = 4; ii > 0; --ii)
        {
            stats.push(float(ii));
            std::cout << stats << std::endl;
        }

        std::cout << "Reset" << std::endl;
        stats.reset();

        std::cout << "Calculating height statistics" << std::endl;
        std::vector<float> heights = {175.2f, 162.6f, 135.2f, 192.5f, 178.8f, 165.5f, 220.3f};
        stats.run(heights.begin(), heights.end());
        std::cout << stats << "cm" << std::endl;
    }

    {
        math::Statistics<double> stats;

        std::cout << "Same with doubles" << std::endl;
        std::vector<double> heights = {175.2, 162.6, 135.2, 192.5, 178.8, 165.5, 220.3};
        stats.run(heights.begin(), heights.end());
        std::cout << stats << "cm" << std::endl;
    }

    return 0;
}
