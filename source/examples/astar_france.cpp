#include "algorithm/astar.h"
#include "fmt/format.h"

#include <cmath>
#include <numbers>
#include <random>

using namespace kb;

/*
    Given a rough map of France, find the shortest path between two cities.

    We'll use the A* algorithm to find the shortest path.
    A city is represented by a vertex in a graph.
    Cities that are connected by a road are considered to be connected in the graph representation.
    Edges in the graph represent the driving distance between two cities.
    Cities are geolocated using their latitude and longitude, we'll use this information to calculate
    the distance as the crow flies between two cities. This distance can be used as the heuristic.
*/

constexpr float k_pi = std::numbers::pi_v<float>;

// Geographic Coordinate System
struct GCS
{
    float lat;
    float lon;

    GCS(float lat_deg, float lon_deg) : lat(lat_deg * k_pi / 180.f), lon(lon_deg * k_pi / 180.f)
    {
    }
};

// Approximate Haversine distance (shortest distance between two points on a sphere)
inline float orthodromic_distance(const GCS& gcs1, const GCS& gcs2)
{
    constexpr float dconv = 111.12f; // 1 degree in km
    float d = acosf(sinf(gcs1.lat) * sinf(gcs2.lat) + cosf(gcs1.lat) * cosf(gcs2.lat) * cosf(gcs1.lon - gcs2.lon));
    return d * (180.f / k_pi) * dconv;
}

// Directed edge from a city to another
struct Edge
{
    size_t to;       // Vertex id in graph
    float road_dist; // Driving distance in km
};

// Vertex in the graph representing a city
struct Vertex
{
    GCS gcs;                       // Latitude and longitude
    std::string name;              // City name
    std::vector<Edge> neighbors{}; // List of neighboring cities with driving distance
};

// Graph representation of the city network
struct Graph
{
    // List of cities
    std::vector<Vertex> vertices;

    inline size_t add_vertex(const std::string& name, const GCS& gcs)
    {
        vertices.push_back({gcs, name});
        return vertices.size() - 1;
    }

    inline void add_edge(size_t from, size_t to, float road_dist)
    {
        vertices[from].neighbors.push_back({to, road_dist});
        vertices[to].neighbors.push_back({from, road_dist});
    }
};

/*
    The search state must be compliant with the AstarState concept in order
    for your code to compile.
*/
struct NavSearchState
{
    Graph* graph;
    size_t vertex_id;

    // A state must be able to compare itself to other states
    bool operator==(const NavSearchState& other) const
    {
        return vertex_id == other.vertex_id;
    }

    // A state must be hashable
    uint64_t hash() const
    {
        return vertex_id;
    }

    // We must be able to calculate the cost of moving to another state from this one
    float transition_cost(const NavSearchState& successor) const
    {
        // Linear search for road distance to successor
        const auto& neighbors = graph->vertices[vertex_id].neighbors;

        for (size_t ii = 0; ii < neighbors.size(); ++ii)
            if (neighbors[ii].to == successor.vertex_id)
                return neighbors[ii].road_dist;

        return std::numeric_limits<float>::infinity();
    }

    // We must be able to estimate the remaining distance from this state to the goal state
    float heuristic(const NavSearchState& goal) const
    {
        // We use the distance as the crow flies to estimate the remaining distance
        return orthodromic_distance(graph->vertices[vertex_id].gcs, graph->vertices[goal.vertex_id].gcs);
    }

    // We must be able to get the successors of this state
    std::vector<NavSearchState> get_successors(const NavSearchState* parent) const
    {
        std::vector<NavSearchState> successors;

        const auto& neighbors = graph->vertices[vertex_id].neighbors;

        // Avoid returning the previous state (guide search)
        for (size_t ii = 0; ii < neighbors.size(); ++ii)
            if (parent == nullptr || (parent->vertex_id != neighbors[ii].to))
                successors.push_back({graph, neighbors[ii].to});

        return successors;
    }
};

// Generate a city network for France
Graph make_france()
{
    Graph graph;

    // Parsing JSON files is for the weak.
    // clang-format off
    size_t amiens =      graph.add_vertex("Amiens",        GCS(49.894066f, 2.295753f));
    size_t angers =      graph.add_vertex("Angers",        GCS(47.471100f, -0.547307f));
    size_t auxerre =     graph.add_vertex("Auxerre",       GCS(47.799999f, 3.566670f));
    size_t bordeaux =    graph.add_vertex("Bordeaux",      GCS(44.833328f, -0.566670f));
    size_t bourges =     graph.add_vertex("Bourges",       GCS(47.081012f, 2.398782f));
    size_t brest =       graph.add_vertex("Brest",         GCS(48.400002f, -4.483330f));
    size_t caen =        graph.add_vertex("Caen",          GCS(49.183333f, -0.350000f));
    size_t calais =      graph.add_vertex("Calais",        GCS(50.950001f, 1.833330f));
    size_t clermont =    graph.add_vertex("Clermont - Fd", GCS(45.783329f, 3.083330f));
    size_t dijon =       graph.add_vertex("Dijon",         GCS(47.316669f, 5.016670f));
    size_t grenoble =    graph.add_vertex("Grenoble",      GCS(45.166672f, 5.716670f));
    size_t le_havre =    graph.add_vertex("Le Havre",      GCS(49.493800f, 0.107700f));
    size_t le_mans =     graph.add_vertex("Le Mans",       GCS(47.988178f, 0.160791f));
    size_t lille =       graph.add_vertex("Lille",         GCS(50.633333f, 3.066667f));
    size_t limoges =     graph.add_vertex("Limoges",       GCS(45.849998f, 1.250000f));
    size_t lyon =        graph.add_vertex("Lyon",          GCS(45.750000f, 4.850000f));
    size_t marseille =   graph.add_vertex("Marseille",     GCS(43.300000f, 5.400000f));
    size_t metz =        graph.add_vertex("Metz",          GCS(49.133333f, 6.166667f));
    size_t montauban =   graph.add_vertex("Montauban",     GCS(44.016670f, 1.350000f));
    size_t montpellier = graph.add_vertex("Montpellier",   GCS(43.625050f, 3.862038f));
    size_t nancy =       graph.add_vertex("Nancy",         GCS(48.683331f, 6.200000f));
    size_t nantes =      graph.add_vertex("Nantes",        GCS(47.216671f, -1.550000f));
    size_t orleans =     graph.add_vertex("Orléans",       GCS(47.916672f, 1.900000f));
    size_t paris =       graph.add_vertex("Paris",         GCS(48.866667f, 2.333333f));
    size_t pau =         graph.add_vertex("Pau",           GCS(43.300000f, -0.366667f));
    size_t poitiers =    graph.add_vertex("Poitiers",      GCS(46.583328f, 0.333330f));
    size_t reims =       graph.add_vertex("Reims",         GCS(49.250000f, 4.033330f));
    size_t rennes =      graph.add_vertex("Rennes",        GCS(48.083328f, -1.683330f));
    size_t rouen =       graph.add_vertex("Rouen",         GCS(49.433331f, 1.083330f));
    size_t toulouse =    graph.add_vertex("Toulouse",      GCS(43.600000f, 1.433333f));
    size_t tours =       graph.add_vertex("Tours",         GCS(47.383333f, 0.683333f));
    size_t valence =     graph.add_vertex("Valence",       GCS(44.933331f, 4.900000f));
    // clang-format on

    graph.add_edge(calais, lille, 110);
    graph.add_edge(calais, amiens, 153);
    graph.add_edge(lille, amiens, 113);
    graph.add_edge(amiens, rouen, 114);
    graph.add_edge(amiens, reims, 170);
    graph.add_edge(rouen, le_havre, 89);
    graph.add_edge(rouen, caen, 131);
    graph.add_edge(rouen, paris, 148);
    graph.add_edge(paris, reims, 180);
    graph.add_edge(reims, metz, 178);
    graph.add_edge(caen, le_mans, 160);
    graph.add_edge(paris, le_mans, 224);
    graph.add_edge(paris, orleans, 123);
    graph.add_edge(metz, nancy, 59);
    graph.add_edge(nancy, dijon, 200);
    graph.add_edge(orleans, auxerre, 160);
    graph.add_edge(auxerre, dijon, 144);
    graph.add_edge(le_mans, rennes, 157);
    graph.add_edge(le_mans, angers, 98);
    graph.add_edge(le_mans, tours, 84);
    graph.add_edge(rennes, nantes, 124);
    graph.add_edge(rennes, angers, 132);
    graph.add_edge(rennes, brest, 251);
    graph.add_edge(brest, nantes, 318);
    graph.add_edge(nantes, angers, 93);
    graph.add_edge(nantes, poitiers, 181);
    graph.add_edge(angers, tours, 108);
    graph.add_edge(tours, orleans, 120);
    graph.add_edge(tours, poitiers, 109);
    graph.add_edge(tours, bourges, 152);
    graph.add_edge(orleans, bourges, 109);
    graph.add_edge(bourges, limoges, 194);
    graph.add_edge(limoges, poitiers, 120);
    graph.add_edge(poitiers, bordeaux, 247);
    graph.add_edge(bordeaux, limoges, 220);
    graph.add_edge(bordeaux, montauban, 216);
    graph.add_edge(limoges, montauban, 257);
    graph.add_edge(bordeaux, pau, 200);
    graph.add_edge(pau, toulouse, 174);
    graph.add_edge(montauban, toulouse, 54);
    graph.add_edge(bourges, clermont, 194);
    graph.add_edge(clermont, lyon, 177);
    graph.add_edge(dijon, lyon, 200);
    graph.add_edge(clermont, montpellier, 339);
    graph.add_edge(montpellier, toulouse, 233);
    graph.add_edge(montpellier, marseille, 173);
    graph.add_edge(lyon, grenoble, 114);
    graph.add_edge(lyon, valence, 103);
    graph.add_edge(grenoble, valence, 99);
    graph.add_edge(valence, marseille, 229);

    return graph;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Graph france = make_france();

    // Select two cities at random
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dis_city(0, france.vertices.size() - 1);
    size_t start_city = dis_city(rng);
    size_t end_city = dis_city(rng);

    fmt::println("Searching for shortest path between {} and {}", france.vertices[start_city].name,
                 france.vertices[end_city].name);

    // These two lines instantiate and run the A* algorithm
    Astar<NavSearchState> astar({&france, start_city}, {&france, end_city}, 1024);
    auto status = astar.search();

    if (status == Astar<NavSearchState>::Status::SUCCESS)
    {
        fmt::println("Success!");
        fmt::println("Steps: {}", astar.get_steps());
        fmt::println("Total distance: {} km", astar.get_solution_cost());

        fmt::print("Path: ");
        // Call this function to visit each node in the path
        astar.walk_path([](const NavSearchState& state) {
            const auto& vertex = state.graph->vertices[state.vertex_id];
            fmt::print("{} ", vertex.name);
        });
        fmt::print("\n");
    }
    else
    {
        fmt::println("Failure.");
    }

    return 0;
}