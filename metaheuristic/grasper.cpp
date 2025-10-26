#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string_view>
#include <unordered_set>

using uint = unsigned int;
using default_clock = std::chrono::steady_clock;
using second_duration = std::chrono::duration<double, std::ratio<1>>;

struct parameters {
    std::string_view input_path;
    uint num_iterations;
    uint seed = std::random_device()();
    double alpha = 0.05;
    double time_control = 0;
};

struct position {
    uint x;
    uint y;
};

struct temple {
    position pos;
    std::unordered_set<uint> prerequisites;
    std::unordered_set<uint> dependents;
};

struct problem {
    uint num_temples;
    temple *temples;
};

struct solution {
    uint value;
    uint *route;
};

struct candidate {
    uint temple_idx;
    uint distance;
};

void print_help()
{
    std::cout << "GRASPer peregrinação solver using GRASP\n";
    std::cout << "usage: grasper <file> <num_iterations> [seed] [alpha] [time_control]\n";
}

// Parses parameter values from the CLI arguments
// <file> <num_iterations> [seed] [alpha] [time_control]
const parameters parse_parameters(int argc, char *argv[])
{
    parameters p;

    switch (argc) {
        case 6:
            p.time_control = std::atof(argv[5]);
        case 5:
            p.alpha = std::atof(argv[4]);
        case 4:
            p.seed = std::atoi(argv[3]);
        case 3:
            p.input_path = argv[1];
            p.num_iterations = std::atoi(argv[2]);
            break;
        default:
            print_help();
            std::cerr << "\nError: Wrong number of parameters\n";
            std::exit(EXIT_FAILURE);
    }

    return p;
}

// Prints the value of each parameter, in a table format
// Won't print time control if not set
void print_parameters(const parameters& p)
{
    std::cout << "Parameters:\n"
              << "Number of iterations  " << p.num_iterations << '\n'
              << "Seed " << std::string(17, ' ') << p.seed << '\n'
              << "Alpha " << std::string(16, ' ') << p.alpha << '\n';

    if (p.time_control)
        std::cout << "Time control (s) " << std::string(5, ' ') << p.time_control << '\n';
}

// Builds a problem from a file
//
// The file should follow this format:
// <num_temples>
// <temple[0].pos.x> <temple[0].pos.y>
// <temple[1].pos.x> <temple[1].pos.y>
// ...
// <temple[num_temples - 1].x> <temple[num_temples - 1].y>
// <num_prerequisites>
// <prerequisite_idx[0]> <dependent_idx[0]>
// <prerequisite_idx[1]> <dependent_idx[1]>
// ...
// <prerequisite_idx[num_prerequisites - 1]> <dependent_idx[num_prerequisites - 1]>
//
// (prerequisite_idx[] and dependent_idx[] are values between 1 and num_temples)
const problem parse_input_file(std::ifstream file)
{
    if (!file) {
        print_help();
        std::cerr << "Error: Could not open file\n";
        std::exit(EXIT_FAILURE);
    }

    problem prob;

    file >> prob.num_temples;
    prob.temples = new temple[prob.num_temples];
    for (uint i = 0; i < prob.num_temples; ++i)
        file >> prob.temples[i].pos.x >> prob.temples[i].pos.y;

    uint num_dependencies, prerequisites, dependents;
    file >> num_dependencies;
    for (uint i = 0; i < num_dependencies; ++i) {
        file >> prerequisites >> dependents;
        prerequisites--; dependents--;
        prob.temples[prerequisites].dependents.insert(dependents);
        prob.temples[dependents].prerequisites.insert(prerequisites);
    }

    return prob;
}

// Computes the distance between two temple positions
// It is the euclidian distance multiplied by 100 floored
uint temples_distance(const position a, const position b)
{
    return std::sqrt((double)((b.x - a.x)*(b.x - a.x) + (b.y - a.y)*(b.y - a.y))) * 100;
}

// Sort function for candidates by the distance
bool candidate_sorter(const candidate& l, const candidate& r)
{
    return l.distance < r.distance;
}

// Chooses the next candidate based on the distance to the last chosen temple
// Builds a Restrictive Candidate List with the alpha% best candidates
// The returned candidate is randomly selected from the RCL
uint choose_candidate(
    const problem& prob,
    candidate *candidates,
    uint num_candidates,
    uint last_chosen_idx,
    double alpha,
    std::mt19937& rng)
{
    // For each candidate, compute the distance to the last chosen temple
    for (uint i = 0; i < num_candidates; ++i) {
        candidates[i].distance = temples_distance(prob.temples[last_chosen_idx].pos,
                                                  prob.temples[candidates[i].temple_idx].pos);
    }

    std::sort(candidates, &candidates[num_candidates], &candidate_sorter);

    // k is the size of the RCL, computed with the alpha term
    uint k = std::max(1, (int) std::ceil(num_candidates * alpha));

    // Randomly select candidate from the RCL
    std::uniform_int_distribution<> dist(0, k - 1);

    return dist(rng);
}

// Builds a greedy randomized solution for the problem
// A copy of the problem should be used as it is modified internally
void greedy_randomized(
    problem& prob,
    solution& sol,
    candidate* candidates,
    double alpha,
    std::mt19937& rng)
{
    uint num_candidates = 0;
    for (uint i = 0; i < prob.num_temples; ++i) {
        if (prob.temples[i].prerequisites.empty())
            candidates[num_candidates++] = {i, 0};
    }

    // The first chosen temple is completely random
    std::uniform_int_distribution<> dist(0, num_candidates - 1);
    uint chosen_candidate_idx = dist(rng);
    candidate chosen_candidate = candidates[chosen_candidate_idx];

    sol.value = 0;
    for (uint route_size = 0; route_size < prob.num_temples - 1; route_size++) {

        sol.value += chosen_candidate.distance;
        sol.route[route_size] = chosen_candidate.temple_idx;

        // Remove the prerequisite from other temples
        for (auto i : prob.temples[chosen_candidate.temple_idx].dependents) {
            prob.temples[i].prerequisites.erase(chosen_candidate.temple_idx);

            // Add temple to the candidates if there is no prerequisite anymore
            if (prob.temples[i].prerequisites.empty())
                candidates[num_candidates++] = {i, 0};
        }

        // Remove chosen candidate from candidates
        candidates[chosen_candidate_idx] = candidates[--num_candidates];

        // Choose the next temple from candidates
        chosen_candidate_idx = choose_candidate(prob, candidates, num_candidates,
                                                chosen_candidate.temple_idx, alpha, rng);
        chosen_candidate = candidates[chosen_candidate_idx];
    }

    // Skip removing the prerequisite of the last chosen temple
    sol.value += chosen_candidate.distance;
    sol.route[prob.num_temples - 1] = chosen_candidate.temple_idx;
}

// Copy problem a to b, which must have already been allocated
void copy_problem(const problem& a, problem& b)
{
    b.num_temples = a.num_temples;
    std::copy(a.temples, &a.temples[a.num_temples], b.temples);
}

// Verify whether the solution respects all prerequisites
bool is_valid_solution(
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
// Only recalculates the distance of the new connections
uint compute_new_sol_value(
    const problem& prob,
    const solution& sol,
    uint idx1,
    uint idx2)
{
    uint new_value = sol.value;

    if (idx2 + 1 < prob.num_temples) {
        new_value -= temples_distance(prob.temples[sol.route[idx2]].pos,
                                      prob.temples[sol.route[idx2 + 1]].pos);
        new_value += temples_distance(prob.temples[sol.route[idx1]].pos,
                                      prob.temples[sol.route[idx2 + 1]].pos);
    }

    if (idx1 > 0) {
        new_value -= temples_distance(prob.temples[sol.route[idx1 - 1]].pos,
                                      prob.temples[sol.route[idx1]].pos);
        new_value += temples_distance(prob.temples[sol.route[idx1 - 1]].pos,
                                      prob.temples[sol.route[idx2]].pos);
    }

    return new_value;
}

// Continually improves the current solution until a local minimum is reached
// Uses a 2-opt neighbourhood
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

                if (!is_valid_solution(prob, sol, i, j, prereq_forward))
                    break;

                cur_value = compute_new_sol_value(prob, sol, i, j);

                if (cur_value < sol.value) {
                    std::reverse(&sol.route[i], &sol.route[j + 1]);
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

// Copy solution a to b, which must have already been allocated
void copy_solution(const struct solution& a, struct solution& b, uint route_size)
{
    b.value = a.value;
    std::copy(a.route, &a.route[route_size], b.route);
}

// Prints the value and route of a solution
void print_solution(const solution& sol, uint route_size)
{
    std::cout << "Solution value: " << sol.value << '\n';

    std::cout << "Solution route: ";
    for (uint i = 0; i < route_size - 1; ++i)
        std::cout << sol.route[i] + 1 << " -> ";
    std::cout << sol.route[route_size - 1] + 1 << '\n';
}

double get_elapsed_time(std::chrono::time_point<default_clock>& timer)
{
    return std::chrono::duration_cast<second_duration>
        (default_clock::now() - timer).count();
}

void print_time(std::chrono::time_point<default_clock>& timer)
{
    std::cout << std::fixed << std::setprecision(2)
              << "Elapsed time: " << get_elapsed_time(timer) << " seconds\n";
}

solution grasp(
    const problem& prob,
    uint num_iterations,
    double alpha,
    double time_control,
    std::mt19937& rng,
    std::chrono::time_point<default_clock>& timer)
{
    solution sol;
    sol.route = new uint[prob.num_temples];

    solution best_sol;
    best_sol.route = new uint[prob.num_temples];
    best_sol.value = std::numeric_limits<uint>::max();

    problem prob_tmp;
    prob_tmp.temples = new temple[prob.num_temples];

    candidate *candidates = new candidate[prob.num_temples];
    bool *prereq_forward = new bool[prob.num_temples];
    uint *search_order = new uint[prob.num_temples - 1];
    std::iota(search_order, &search_order[prob.num_temples - 1], 0);

    for (uint i = 0; i < num_iterations; ++i) {
        if (time_control && get_elapsed_time(timer) > time_control) {
            time_control = 0;
            std::cout << '\n';
            std::cout << "Iteration " << i << " started" << '\n';
            print_time(timer);
        }

        copy_problem(prob, prob_tmp);
        greedy_randomized(prob_tmp, sol, candidates, alpha, rng);
        local_search(sol, prob, prereq_forward, search_order, rng);

        if (sol.value < best_sol.value) {
            copy_solution(sol, best_sol, prob.num_temples);
            std::cout << '\n';
            print_time(timer);
            print_solution(sol, prob.num_temples);
        }
    }

    delete[] sol.route;
    delete[] prob_tmp.temples;
    delete[] candidates;
    delete[] prereq_forward;
    delete[] search_order;

    return best_sol;
}

int main(int argc, char *argv[])
{
    parameters params = parse_parameters(argc, argv);
    print_parameters(params);

    std::mt19937 rng(params.seed);

    std::chrono::time_point<default_clock> timer{default_clock::now()};

    const problem prob = parse_input_file(std::ifstream(params.input_path.data()));

    const solution best_sol = grasp(prob,
                                     params.num_iterations,
                                     params.alpha,
                                     params.time_control,
                                     rng,
                                     timer);

    delete[] prob.temples;
    delete[] best_sol.route;

    return 0;
}
