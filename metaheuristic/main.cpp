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

#define DEFAULT_ALPHA 0.1

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
    return std::sqrt((double)((b.x - a.x)*(b.x - a.x) + (b.y - a.y)*(b.y - a.y))) * 100;
}

uint compute_solution_value(const struct problem& prob, const struct solution& sol)
{
    uint value = 0;

    for (uint i = 0; i + 1 < prob.num_temples; ++i) {
        value += temples_distance(prob.temples[sol.route[i]], prob.temples[sol.route[i + 1]]);
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

// Sorts candidates by the distance
bool candidate_sorter(const struct candidate& l, const struct candidate& r)
{
    return l.distance < r.distance;
}

// Builds a Restrictive Candidate List based on alpha
std::vector<struct candidate> build_rcl(
    const struct problem prob,
    const std::set<uint>& candidates,
    uint last_chosen,
    double alpha)
{
    std::vector<struct candidate> candidates_distances;

    // For each candidate, compute the distance to the last temple
    for (uint c : candidates) {
        candidates_distances.push_back({c,
            temples_distance(prob.temples[last_chosen], prob.temples[c])
        });
    }

    auto [min_it, max_it] = std::minmax_element(
        candidates_distances.begin(), candidates_distances.end(), &candidate_sorter);

    double min = min_it->distance;
    double max = max_it->distance;
    double threshold = min + alpha * (max - min);

    std::vector<struct candidate> rcl;
    for (const auto& c : candidates_distances) {
        if (c.distance <= threshold)
            rcl.push_back(c);
    }

    return rcl;
}

// Builds a greedy_randomized solution for the problem
// A copy of the problem should be used as it is modified internally
void greedy_randomized(
    struct problem& prob,
    struct solution& sol,
    double alpha,
    std::mt19937& rng)
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
    struct candidate chosen = {first_candidates[dist(rng)], 0};
    struct candidate prev_chosen = chosen;

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
        std::vector<struct candidate> rcl = build_rcl(prob, candidates, chosen.idx, alpha);
        std::uniform_int_distribution<> dist(0, rcl.size() - 1);
        chosen = rcl[dist(rng)];
    }

    sol.value += chosen.distance;
    sol.route[prob.num_temples - 1] = chosen.idx;
}

// Copy problem a to b, which must have already been allocated
void copy_problem(const struct problem& a, struct problem& b)
{
    b.num_temples = a.num_temples;
    for (uint i = 0; i < a.num_temples; ++i)
        b.temples[i] = a.temples[i];
}

bool check_valid_sol(
    const struct problem& prob,
    uint idx1,
    uint idx2,
    std::vector<bool>& prereq_forward)
{
    bool valid_sol = true;

    for (uint i = idx2; i > idx1 && valid_sol; i--) {
        for (uint prereq : prob.temples[i].prerequisites) {
            if (prereq_forward[prereq]) {
                valid_sol = false;
                break;
            }
        }
    }

    return valid_sol;
}

void local_search(struct solution& sol, const struct problem& prob)
{
    uint temp_value;
    bool was_improvement = true;
    std::vector<bool> prereq_forward(prob.num_temples, false);

    while (was_improvement) {
        was_improvement = false;

        for (uint i = 0; i < prob.num_temples - 1 && !was_improvement; i++) {

            prereq_forward[sol.route[i]] = true;

            for (uint j = i + 1; j < prob.num_temples; j++) {

                if(check_valid_sol(prob, i, j, prereq_forward)) {
                    std::reverse(sol.route.begin() + i, sol.route.begin() + j + 1);

                    temp_value = compute_solution_value(prob, sol);

                    if(temp_value < sol.value) {
                        sol.value = temp_value;
                        was_improvement = true;
                        break;
                    }
                    else {
                        std::reverse(sol.route.begin() + i, sol.route.begin() + j + 1);
                    }

                }
                else {
                    break;
                }

                prereq_forward[sol.route[j]] = true;
            }

            std::fill(prereq_forward.begin(), prereq_forward.end(), false);
        }
    }
}

struct solution grasp(
    const struct problem& prob,
    uint num_iterations,
    double alpha,
    std::mt19937& rng)
{
    struct solution sol(prob.num_temples);
    struct solution best_sol(prob.num_temples);
    best_sol.value = std::numeric_limits<uint>::max();

    struct problem prob_tmp;
    prob_tmp.temples = new struct temple[prob.num_temples];
    copy_problem(prob, prob_tmp);

    for (uint i = 0; i < num_iterations; ++i) {
        greedy_randomized(prob_tmp, sol, alpha, rng);
        copy_problem(prob, prob_tmp);

        local_search(sol, prob);

        if (sol.value < best_sol.value) {
            best_sol = sol;
            // TODO: print elapsed time
            print_solution(sol, prob.num_temples);
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

    const struct problem prob = parse_input(std::ifstream(input_path.data()));

    grasp(prob, num_iterations, alpha, rng);

    return 0;
}
