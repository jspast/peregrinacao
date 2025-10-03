#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <set>

using uint = unsigned int;
using default_clock = std::chrono::steady_clock;
using second_duration = std::chrono::duration<double, std::ratio<1> >;

#define DEFAULT_ALPHA 0.1

struct temple {
    uint x;
    uint y;
    std::unordered_set<uint> prerequisites;
    std::unordered_set<uint> dependents;
};

struct problem {
    uint num_temples;
    temple *temples;
};

struct solution {
    uint value;
    std::vector<uint> route;

    solution(uint size) : route(size) {}
};

struct candidate {
    uint idx;
    uint distance;
};

void print_help()
{
    std::cout << "GRASPer peregrinação solver using GRASP\n";
    std::cout << "usage: grasper [file] [num_iterations] [seed]\n";
}

const problem& parse_input(std::ifstream file)
{
    if (!file) {
        print_help();
        std::cerr << "Error: Could not open file\n";
        std::exit(EXIT_FAILURE);
    }

    problem *prob = new problem;

    file >> prob->num_temples;
    prob->temples = new temple[prob->num_temples];
    for (uint i = 0; i < prob->num_temples; ++i)
        file >> prob->temples[i].x >> prob->temples[i].y;

    uint num_dependencies, prerequisites, dependents;
    file >> num_dependencies;
    for (uint i = 0; i < num_dependencies; ++i) {
        file >> prerequisites >> dependents;
        prerequisites--; dependents--;
        prob->temples[prerequisites].dependents.insert(dependents);
        prob->temples[dependents].prerequisites.insert(prerequisites);
    }

    return *prob;
}

uint temples_distance(const temple& a, const temple& b)
{
    return std::sqrt((double)((b.x - a.x)*(b.x - a.x) + (b.y - a.y)*(b.y - a.y))) * 100;
}

void print_time(std::chrono::time_point<default_clock>& timer)
{
    double elapsed_time = std::chrono::duration_cast<second_duration>
        (default_clock::now() - timer).count();
    std::cout << std::setprecision(2) << "Elapsed time: " << elapsed_time << " seconds\n";
}

void print_solution(const solution& sol)
{
    std::cout << "Solution value: " << sol.value << '\n';

    std::cout << "Solution route: ";
    for (uint i = 0; i < sol.route.size() - 1; ++i)
        std::cout << sol.route[i] + 1 << " -> ";
    std::cout << sol.route[sol.route.size() - 1] + 1 << '\n';
}

// Sorts candidates by the distance
bool candidate_sorter(const candidate& l, const candidate& r)
{
    return l.distance < r.distance;
}

// Builds a Restrictive Candidate List based on alpha
std::vector<candidate> build_rcl(
    const problem prob,
    const std::set<uint>& candidates,
    uint last_chosen,
    double alpha)
{
    std::vector<candidate> candidates_distances;
    uint min = std::numeric_limits<uint>::max();
    uint max = std::numeric_limits<uint>::min();

    // For each candidate, compute the distance to the last temple
    for (uint c : candidates) {
        uint distance = temples_distance(prob.temples[last_chosen], prob.temples[c]);
        candidates_distances.push_back({c, distance});

        min = std::min(min, distance);
        max = std::max(max, distance);
    }

    double threshold = min + alpha * (max - min);

    std::vector<candidate> rcl;
    for (const auto& c : candidates_distances) {
        if (c.distance <= threshold)
            rcl.push_back(c);
    }

    return rcl;
}

// Builds a greedy_randomized solution for the problem
// A copy of the problem should be used as it is modified internally
void greedy_randomized(problem& prob, solution& sol, double alpha, std::mt19937& rng)
{
    sol.value = 0;

    // Use vector for the first choice as it is better for selecting a random element
    std::vector<uint> first_candidates;
    std::set<uint> candidates;

    for (uint i = 0; i < prob.num_temples; ++i) {
        if (prob.temples[i].prerequisites.empty()) {
            candidates.insert(i);
            first_candidates.push_back(i);
        }
    }

    // The first chosen temple is completely random
    std::uniform_int_distribution<> dist(0, first_candidates.size() - 1);
    candidate chosen = {first_candidates[dist(rng)], 0};
    candidate prev_chosen = chosen;

    for (uint sol_size = 0; sol_size < prob.num_temples - 1; sol_size++) {

        sol.value += chosen.distance;
        sol.route[sol_size] = chosen.idx;

        // Remove the prerequisite from other temples
        for (auto i : prob.temples[chosen.idx].dependents) {
            prob.temples[i].prerequisites.erase(chosen.idx);

            // Add temple to the candidates if there is no prerequisite anymore
            if (prob.temples[i].prerequisites.empty())
                candidates.insert(i);
        }

        // Remove the current temple from the candidates
        candidates.erase(chosen.idx);
        prev_chosen = chosen;

        // Choose the next temple with a Restrictive Candidate List
        std::vector<candidate> rcl = build_rcl(prob, candidates, chosen.idx, alpha);
        std::uniform_int_distribution<> dist(0, rcl.size() - 1);
        chosen = rcl[dist(rng)];
    }

    sol.value += chosen.distance;
    sol.route[prob.num_temples - 1] = chosen.idx;
}

// Copy problem a to b, which must have already been allocated
void copy_problem(const problem& a, problem& b)
{
    b.num_temples = a.num_temples;
    std::copy(a.temples, &a.temples[a.num_temples], b.temples);
}

bool check_valid_sol(
    const problem& prob,
    const solution& sol,
    uint idx1,
    uint idx2,
    bool *prereq_forward)
{
    bool valid_sol = true;

    for (uint i = idx2; i > idx1 && valid_sol; i--) {
        for (uint prereq : prob.temples[sol.route[i]].prerequisites) {
            if (prereq_forward[prereq]) {
                valid_sol = false;
                break;
            }
        }
    }

    return valid_sol;
}

// Efficiently computes the solution value from a 2-opt operation
uint compute_new_sol_value(
    const problem& prob,
    const solution& sol,
    uint idx1,
    uint idx2)
{
    uint new_value = sol.value;

    if (idx2 + 1 < prob.num_temples) {
        new_value -= temples_distance(prob.temples[sol.route[idx2]],
                                      prob.temples[sol.route[idx2 + 1]]);
        new_value += temples_distance(prob.temples[sol.route[idx1]],
                                      prob.temples[sol.route[idx2 + 1]]);
    }

    if (idx1 > 0) {
        new_value -= temples_distance(prob.temples[sol.route[idx1 - 1]],
                                      prob.temples[sol.route[idx1]]);
        new_value += temples_distance(prob.temples[sol.route[idx1 - 1]],
                                      prob.temples[sol.route[idx2]]);
    }

    return new_value;
}

void local_search(
    solution& sol,
    const problem& prob,
    bool *prereq_forward,
    uint *search_order,
    std::mt19937& rng)
{
    uint cur_value;
    bool was_improvement = true;

    while (was_improvement) {
        was_improvement = false;

        // Explore the neighbourhood in a different order each time
        std::shuffle(search_order, &search_order[prob.num_temples - 1], rng);

        for (uint k = 0; k < prob.num_temples - 1 && !was_improvement; k++) {
            uint i = search_order[k];

            prereq_forward[sol.route[i]] = true;

            for (uint j = i + 1; j < prob.num_temples; j++) {

                if (!check_valid_sol(prob, sol, i, j, prereq_forward))
                    break;

                cur_value = compute_new_sol_value(prob, sol, i, j);

                if (cur_value < sol.value) {
                    std::reverse(sol.route.begin() + i, sol.route.begin() + j + 1);
                    sol.value = cur_value;
                    was_improvement = true;
                    break;
                }

                prereq_forward[sol.route[j]] = true;
            }

            std::fill(prereq_forward, &prereq_forward[prob.num_temples], false);
        }
    }
}

solution grasp(
    const problem& prob,
    uint num_iterations,
    double alpha,
    std::mt19937& rng,
    std::chrono::time_point<default_clock>& timer)
{
    solution sol(prob.num_temples);
    solution best_sol(prob.num_temples);
    best_sol.value = std::numeric_limits<uint>::max();

    problem prob_tmp;
    prob_tmp.temples = new temple[prob.num_temples];

    bool *prereq_forward = new bool[prob.num_temples];
    uint *search_order = new uint[prob.num_temples - 1];
    std::iota(search_order, &search_order[prob.num_temples - 1], 0);

    for (uint i = 0; i < num_iterations; ++i) {
        copy_problem(prob, prob_tmp);
        greedy_randomized(prob_tmp, sol, alpha, rng);
        local_search(sol, prob, prereq_forward, search_order, rng);

        if (sol.value < best_sol.value) {
            best_sol = sol;
            print_time(timer);
            print_solution(sol);
        }
    }

    return best_sol;
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
    std::mt19937 rng(seed);

    double alpha = argc > 4 ? std::atof(argv[4]) : DEFAULT_ALPHA;

    std::chrono::time_point<default_clock> timer{default_clock::now()};

    const problem prob = parse_input(std::ifstream(input_path.data()));

    grasp(prob, num_iterations, alpha, rng, timer);

    return 0;
}
