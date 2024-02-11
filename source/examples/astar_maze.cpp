#include "algorithm/astar.h"
#include "fmt/format.h"
#include <random>
#include <set>

using namespace kb;

/*
    This example shows how to use the A* algorithm to find the
    shortest path between two points in a maze.
*/

constexpr int32_t k_width = 16;
constexpr int32_t k_height = 16;

/*
    The map of the maze. There is a small enclosed space, so the
    algorithm may fail.
    0 = walkable, 1 = wall
*/
uint32_t world_map[size_t(k_width * k_height)] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
    0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, // 1
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, // 2
    0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, // 3
    0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, // 4
    0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, // 5
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, // 6
    0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, // 7
    0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, // 8
    0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, // 9
    0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, // 10
    0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, // 11
    0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, // 12
    1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, // 13
    1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, // 14
    1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, // 15
};

bool is_walkable(int32_t x, int32_t y)
{
    // Out of bounds
    if (x < 0 || x >= k_width || y < 0 || y >= k_height)
    {
        return false;
    }

    // Not on a wall
    return world_map[size_t(y * k_width + x)] == 0;
}

/*
    The search state must be compliant with the AstarState concept in order
    for your code to compile.
*/
struct MapSearchState
{
    // User data
    int32_t x = 0;
    int32_t y = 0;

    // A state must be able to compare itself to other states
    bool operator==(const MapSearchState& other) const
    {
        return x == other.x && y == other.y;
    }

    // A state must be hashable
    uint64_t hash() const
    {
        return uint64_t(x) << 32 | uint64_t(y);
    }

    // We must be able to calculate the cost of moving to another state from this one
    float transition_cost(const MapSearchState& successor) const
    {
        /*
            In more complex senarios, we may want to use this state's data in conjunction
            with the successor's data to calculate the cost of moving to the
            successor state.
            Because we guarantee in get_successors() that the successor is walkable, we
            can simply return a fixed cost here.
        */

        // return 1.f + 10.f * float(!is_walkable(successor.x, successor.y));

        (void)successor;
        return 1.f;
    }

    // We must be able to estimate the remaining distance from this state to the goal state
    float heuristic(const MapSearchState& goal) const
    {
        // Basic Manhattan distance
        return float(abs(x - goal.x) + abs(y - goal.y));
    }

    // We must be able to get the successors of this state
    std::vector<MapSearchState> get_successors(const MapSearchState* parent) const
    {
        // Return walkable neighbors, avoid returning the previous state (guide search)
        std::vector<MapSearchState> successors;
        successors.reserve(4);

        if (is_walkable(x - 1, y) && !(parent && (*parent == MapSearchState{x - 1, y})))
            successors.push_back({x - 1, y});

        if (is_walkable(x, y - 1) && !(parent && (*parent == MapSearchState{x, y - 1})))
            successors.push_back({x, y - 1});

        if (is_walkable(x + 1, y) && !(parent && (*parent == MapSearchState{x + 1, y})))
            successors.push_back({x + 1, y});

        if (is_walkable(x, y + 1) && !(parent && (*parent == MapSearchState{x, y + 1})))
            successors.push_back({x, y + 1});

        return successors;
    }
};

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Calculate random start and goal positions that are not in a wall
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int32_t> dist_x(0, k_width);
    std::uniform_int_distribution<int32_t> dist_y(0, k_height);

#if 0
    // Force start in enclosed area
    int32_t start_x = 2;
    int32_t start_y = 8;
#else
    int32_t start_x = -1;
    int32_t start_y = -1;
#endif
    int32_t goal_x = -1;
    int32_t goal_y = -1;

    while (!is_walkable(start_x, start_y))
    {
        start_x = dist_x(rng);
        start_y = dist_y(rng);
    }

    while (!is_walkable(goal_x, goal_y))
    {
        goal_x = dist_x(rng);
        goal_y = dist_y(rng);
    }

    fmt::println("Searching path from ({}, {}) to ({}, {})", start_x, start_y, goal_x, goal_y);

    // These two lines instantiate and run the A* algorithm
    Astar<MapSearchState> astar({start_x, start_y}, {goal_x, goal_y}, 1024);
    auto status = astar.search();

    if (status == Astar<MapSearchState>::Status::SUCCESS)
    {
        fmt::println("Success!");
        fmt::println("Steps: {}", astar.get_steps());
        fmt::println("Cost:  {}", astar.get_solution_cost());
        std::set<std::pair<int32_t, int32_t>> in_path;

        fmt::print("Path: ");
        // Call this function to visit each node in the path
        astar.walk_path([&in_path](const MapSearchState& state) {
            in_path.insert({state.x, state.y});
            fmt::print("({}, {}) ", state.x, state.y);
        });
        fmt::print("\n");

        // Display world map with path
        for (int32_t yy = 0; yy < k_height; ++yy)
        {
            for (int32_t xx = 0; xx < k_height; ++xx)
            {
                char c = is_walkable(xx, yy) ? ' ' : '#';

                if (in_path.contains({xx, yy}))
                    c = '.';
                if (xx == start_x && yy == start_y)
                    c = 'S';
                if (xx == goal_x && yy == goal_y)
                    c = 'G';

                fmt::print("{} ", c);
            }
            fmt::print("\n");
        }
    }
    else
    {
        fmt::println("Failed.");
    }

    return 0;
}