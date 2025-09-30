#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string_view>
#include <vector>
#include <set>

using uint = unsigned int;

struct temple {
    uint x;
    uint y;
    std::set<uint> prerequisites;
    std::set<uint> dependents;
};

struct problem {
    uint num_temples;
    struct temple *temples;
};

struct solution {
    uint value;
    uint *route;
};

struct neighbour {
    uint idx;
    uint distance;
};

void print_help()
{
    std::cout << "GRASPer peregrinação solver using GRASP\n";
    std::cout << "usage: grasper [file] [num_iterations] [seed]\n";
}

const struct problem& parse_input(std::ifstream file)
{
    if (!file) {
        print_help();
        std::cerr << "Error: Could not open file\n";
        std::exit(EXIT_FAILURE);
    }

    struct problem *prob = new struct problem;

    file >> prob->num_temples;
    prob->temples = new struct temple[prob->num_temples];
    for (uint i = 0; i < prob->num_temples; ++i)
        file >> prob->temples[i].x >> prob->temples[i].y;

    uint dependencies, prerequisites, dependents;
    file >> dependencies;
    for (uint i = 0; i < dependencies; ++i) {
        file >> prerequisites >> dependents;
        prerequisites--; dependents--;
        prob->temples[prerequisites].dependents.insert(dependents);
        prob->temples[dependents].prerequisites.insert(prerequisites);
    }

    return *prob;
}

uint temples_distance(const struct temple& a, const struct temple& b)
{
    return std::sqrt((float)((b.x - a.x)*(b.x - a.x) + (b.y - a.y)*(b.y - a.y))) * 100;
}

uint compute_solution_value(const struct problem& prob, const struct solution& sol)
{
    uint value = 0;

    for (uint i = 0; i+1 < prob.num_temples; ++i) {
        value += temples_distance(prob.temples[sol.route[i]], prob.temples[sol.route[i+1]]);
    }

    return value;
}

void print_solution(const struct solution& sol, const uint sol_size)
{
    std::cout << "Solution value: " << sol.value << '\n';

    std::cout << "Solution route: ";
    for (uint i = 0; i < sol_size - 1; ++i)
        std::cout << sol.route[i] + 1 << " -> ";
    std::cout << sol.route[sol_size - 1] + 1 << '\n';
}

// For debbuging purposes, prints <idx>:<distance> for each neighbour
void print_neighbourhood(std::vector<struct neighbour> neighbourhood)
{
    for (auto n : neighbourhood) {
        std::cout << n.idx << ':' << n.distance << ' ';
    }
    std::cout << '\n';
}

// Sorts neighbours by the distance
bool neighbour_sorter(const struct neighbour& l, const struct neighbour& r)
{
    return l.distance < r.distance;
}

// Builds a greedy_randomized solution for the problem
// A copy of the problem should be used as it is modified internally
void greedy_randomized(
    struct problem& prob,
    struct solution& sol,
    float alpha,
    uint k,
    std::default_random_engine rng)
{
    std::vector<struct neighbour> neighbourhood;

    for (uint i = 0; i < prob.num_temples; ++i) {
        if (prob.temples[i].prerequisites.empty())
            neighbourhood.push_back({i, 0});
    }

    // The first temple should be chosen randomly
    std::uniform_int_distribution<> dist(0, neighbourhood.size());
    uint chosen = dist(rng);

    for (uint sol_size = 0; sol_size < prob.num_temples; sol_size++) {

        // Debugging:
        // print_neighbourhood(neighbourhood);
        // std::cout << neighbourhood[chosen].idx << '\t'
        //           << prob.temples[neighbourhood[chosen].idx].x << '\t'
        //           << neighbourhood[chosen].distance << "\t| "
        //           << chosen << '\t' << neighbourhood.size() << '\n';

        uint cur_temple_idx = neighbourhood[chosen].idx;
        sol.route[sol_size] = cur_temple_idx;

        // Remove the prerequisite from other temples
        for (auto i : prob.temples[cur_temple_idx].dependents) {
            prob.temples[i].prerequisites.erase(cur_temple_idx);

            // Bring temple to the neighbourhood if there is no prerequisite anymore
            if (prob.temples[i].prerequisites.empty())
                neighbourhood.push_back({i, 0});
        }

        // For each neighbour, update the distance to the current temple
        for (uint i= 0; i < neighbourhood.size(); ++i) {
            neighbourhood[i].distance = temples_distance(
                prob.temples[cur_temple_idx],
                prob.temples[neighbourhood[i].idx]
            );
        }

        // Remove the current temple from the neighbourhood
        neighbourhood.erase(neighbourhood.begin() + chosen);

        // Sort neighbours by the distance
        std::sort(neighbourhood.begin(), neighbourhood.end(), &neighbour_sorter);

        // Choose the next temple from the neighbourhood
        // TODO: implement alpha term
        std::uniform_int_distribution<> dist(0, std::min((uint)neighbourhood.size() -1, k -1));
        uint chosen = dist(rng);

        // TODO: keep track of solution's value
    }
}

struct solution& grasp(
    const struct problem& prob,
    uint num_iterations,
    float alpha,
    uint k,
    std::default_random_engine rng)
{
    struct solution *sol = new struct solution;
    sol->route = new uint[prob.num_temples]{0};

    struct solution *best_sol = new struct solution;
    best_sol->route = new uint[prob.num_temples]{0};
    best_sol->value = std::numeric_limits<uint>::max();

    struct problem prob_tmp = prob;

    for (uint i = 0; i < num_iterations; ++i) {
        greedy_randomized(prob_tmp, *sol, alpha, k, rng);
        prob_tmp = prob;

        // TODO: local search

        sol->value = compute_solution_value(prob, *sol);
        if (sol->value < best_sol->value) {
            *best_sol = *sol;
            // TODO: print elapsed time
            print_solution(*sol, prob.num_temples);
        }
    }

    return *best_sol;
}

int main(int argc, char *argv[])
{
    std::string_view input_path;
    uint num_iterations;
    uint seed;

    if (argc < 4) {
        print_help();
        std::cerr << "\nError: insufficient number of parameters\n";
        std::exit(EXIT_FAILURE);
    }

    input_path = argv[1];
    num_iterations = std::atoi(argv[2]);
    seed = std::atoi(argv[3]);
    std::default_random_engine rng(seed);

    const struct problem prob = parse_input(std::ifstream(input_path.data()));

    grasp(prob, num_iterations, 1, 3, rng);

    return 0;
}
