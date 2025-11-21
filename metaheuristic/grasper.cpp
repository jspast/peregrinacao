#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
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
    long iterations_limit;
    uint seed = std::random_device()();
    double alpha = 0.05;
    double time_limit = 0;
    bool generate_results = false;
    bool generate_pre_results = false;
};

struct runtime_info {
    std::chrono::time_point<default_clock> timer{default_clock::now()};
    double time_limit = 0;
    long total_iterations = 0;
    long grasp_iterations = 0;
    uint initial_sol_value = std::numeric_limits<uint>::max();
};

struct position {
    uint x;
    uint y;
};

struct temple {
    position pos;
    std::unordered_set<uint> prerequisites;
    std::vector<uint> dependents;
};

struct problem {
    uint num_temples;
    temple *temples;
    uint *distances;
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
    std::cout << "GRASPer peregrinação solver using GRASP\n"
              << "Usage: grasper <file> [iterations_limit] [seed] [ADDITIONAL_OPTIONS]\n"
              << "Example: grasper 01.txt 50000 1 -a=0.2\n\n"
              << "Additional options:\n"
              << "-a, --alpha=FLOAT\n"
              << "-t, --time-limit=FLOAT\n"
              << "    --generate-results\n"
              << "    --generate-pre-results\n";
}

// Parses parameter values from the CLI arguments
// <file> [iterations_limit] [seed] [ADDITIONAL_OPTIONS]
const parameters parse_parameters(int argc, char *argv[])
{
    if (argc < 3) {
        print_help();
        std::cerr << "\nError: Wrong number of parameters\n";
        std::exit(EXIT_FAILURE);
    }

    parameters p;

    // Required file parameter
    p.input_path = argv[1];

    int parameter_pos = 2;

    for (int i = parameter_pos; i < argc; ++i) {
        if (std::strncmp(argv[i], "-", 1) != 0) {
            if (i == 2) {
                p.iterations_limit = std::stoul(argv[i]);
            }
            else if (i == 3) {
                p.seed = std::stoi(argv[i]);
            }
            else {
                print_help();
                std::cerr << "\nError: Unexpected parameter: " << argv[i] << "\n";
                std::exit(EXIT_FAILURE);
            }
        }
        // Check alpha
        else if (std::strncmp(argv[i], "--alpha=", 8) == 0) {
            p.alpha = std::stod(&argv[i][8]);
        }
        else if (std::strncmp(argv[i], "-a=", 3) == 0) {
            p.alpha = std::stod(&argv[i][3]);
        }
        // Check time-limit
        else if (std::strncmp(argv[i], "--time-limit=", 13) == 0) {
            p.time_limit = std::stod(&argv[i][13]);
        }
        else if (std::strncmp(argv[i], "-t=", 3) == 0) {
            p.time_limit = std::stod(&argv[i][3]);
        }
        // Check generate-results
        else if (std::strncmp(argv[i], "--generate-results", 18) == 0) {
            p.generate_results = true;
        }
                // Check generate-results
        else if (std::strncmp(argv[i], "--generate-pre-results", 22) == 0) {
            p.generate_pre_results = true;
        }
    }

    return p;
}

// Prints the value of each parameter, in a table format
// Won't print time control if not set
void print_parameters(const parameters& p)
{
    std::cout << "Parameters:\n"
              << "Number of iterations  " << p.iterations_limit << '\n'
              << "Seed                  " << p.seed << '\n'
              << "Alpha                 " << p.alpha << '\n';

    if (p.time_limit)
        std::cout << "Time control (s)      " << p.time_limit << '\n';
}

// Computes the distance between two temple positions
// It is the euclidian distance multiplied by 100 floored
inline uint temples_distance(const position a, const position b)
{
    const int dx = b.x - a.x;
    const int dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy) * 100;
}

// Fills the prob.distances matrix
// prob.distances[i * prob.num_temples + j] will have the distance between temple i an j
// prob.distances[j * prob.num_temples + i] will also have that distance
void compute_distance_matrix(problem& prob)
{
    for (uint i = 0; i < prob.num_temples; ++i) {
        for (uint j = i + 1; j < prob.num_temples; ++j) {
            uint distance = temples_distance(prob.temples[i].pos, prob.temples[j].pos);
            prob.distances[i * prob.num_temples + j] = distance;
            prob.distances[j * prob.num_temples + i] = distance;
        }
    }
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
const problem parse_input_file(std::ifstream& file)
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
        prerequisites--; dependents--; // For 0-based indexing
        prob.temples[prerequisites].dependents.push_back(dependents);
        prob.temples[dependents].prerequisites.insert(prerequisites);
    }

    // Ensure there are no duplicates on dependents vector
    for (uint i = 0; i < prob.num_temples; ++i) {
        std::sort(prob.temples[i].dependents.begin(), prob.temples[i].dependents.end());

        prob.temples[i].dependents.erase(
            std::unique(prob.temples[i].dependents.begin(), prob.temples[i].dependents.end()),
            prob.temples[i].dependents.end());
    }

    prob.distances = new uint[prob.num_temples * prob.num_temples];
    compute_distance_matrix(prob);

    return prob;
}

// Sort function for candidates by the distance
inline bool candidate_sorter(const candidate& l, const candidate& r)
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
    for (uint i = 0; i < num_candidates; ++i)
        candidates[i].distance = prob.distances[last_chosen_idx * prob.num_temples +
                                                candidates[i].temple_idx];

    std::sort(candidates, &candidates[num_candidates], &candidate_sorter);

    // k is the size of the RCL, computed with the alpha term
    uint k = std::max(1, (int) std::ceil(num_candidates * alpha));

    // Randomly select candidate from the RCL
    std::uniform_int_distribution<> dist(0, k - 1);

    return dist(rng);
}

inline double get_elapsed_time(const std::chrono::time_point<default_clock>& timer)
{
    return std::chrono::duration_cast<second_duration>
        (default_clock::now() - timer).count();
}

void print_time(const std::chrono::time_point<default_clock>& timer)
{
    std::cout << std::fixed << std::setprecision(2)
              << "Elapsed time: " << get_elapsed_time(timer) << " seconds\n";
}

// Returns true and prints the current iteration and time if timer reached time_limit
// Returns false if not
inline bool check_time_limit(runtime_info info)
{
    if (info.time_limit && get_elapsed_time(info.timer) > info.time_limit) {
        info.time_limit = 0;
        std::cout << "\nLast iteration: " << info.total_iterations - 1 << '\n';
        print_time(info.timer);
        return true;
    }
    else {
        return false;
    }
}

// Builds a greedy randomized solution for the problem
// A copy of the problem should be used as it is modified internally
// Will build an incomplete solution if info.time_limit is reached
// Returns whether the solution is complete
bool greedy_randomized(
    problem& prob,
    solution& sol,
    candidate* candidates,
    double alpha,
    runtime_info& info,
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
        if (check_time_limit(info))
            return false;

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

    return true;
}

// Copy problem a to b, which must have already been allocated
void copy_problem(const problem& a, problem& b)
{
    b.num_temples = a.num_temples;
    std::copy(a.temples, &a.temples[a.num_temples], b.temples);
    std::copy(a.distances, &a.distances[a.num_temples * a.num_temples], b.distances);
}

// Verify whether the solution respects all prerequisites
inline bool is_valid_solution(
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
inline uint compute_new_sol_value(
    const problem& prob,
    const solution& sol,
    uint idx1,
    uint idx2)
{
    uint new_value = sol.value;

    if (idx2 + 1 < prob.num_temples) {
        new_value -= prob.distances[sol.route[idx2] * prob.num_temples +
                                    sol.route[idx2 + 1]];
        new_value += prob.distances[sol.route[idx1] * prob.num_temples +
                                    sol.route[idx2 + 1]];
    }

    if (idx1 > 0) {
        new_value -= prob.distances[sol.route[idx1 - 1] * prob.num_temples +
                                    sol.route[idx1]];
        new_value += prob.distances[sol.route[idx1 - 1] * prob.num_temples +
                                    sol.route[idx2]];
    }

    return new_value;
}

// Improves the current solution until a local minimum or a limit is reached
// Limits can be max_iterations or info.time_limit
// Uses a 2-opt neighbourhood
// Returns the number of iterations left
long local_search(
    solution& sol,
    const problem& prob,
    bool *prereq_forward,
    uint *search_order,
    long iterations_limit,
    runtime_info& info,
    std::mt19937& rng)
{
    bool was_improvement = true;
    long iterations_left = iterations_limit;

    while (was_improvement && iterations_left > 0) {
        was_improvement = false;

        // Explore the neighbourhood in a different order each time
        std::shuffle(search_order, &search_order[prob.num_temples - 1], rng);

        for (uint k = 0; k < prob.num_temples - 1; k++) {
            if (iterations_left <= 0 || was_improvement)
                break;

            uint i = search_order[k];

            prereq_forward[sol.route[i]] = true;
            uint num_set = 1; // Track how many elements we set to true

            for (uint j = i + 1; j < prob.num_temples; j++) {
                if (check_time_limit(info))
                    iterations_left = 1; // Effectively stop local search

                iterations_left--; info.total_iterations++;
                if (iterations_left <= 0)
                    break;

                if (!is_valid_solution(prob, sol, i, j, prereq_forward))
                    break;

                uint cur_value = compute_new_sol_value(prob, sol, i, j);

                if (cur_value < sol.value) {
                    std::reverse(&sol.route[i], &sol.route[j + 1]);
                    sol.value = cur_value;
                    was_improvement = true;
                    break;
                }

                prereq_forward[sol.route[j]] = true;
                num_set++;
            }

            // Only reset the elements we actually set
            for (uint idx = i; idx < i + num_set; idx++) {
                prereq_forward[sol.route[idx]] = false;
            }
        }
    }

    return iterations_left;
}

// Copy solution a to b, which must have already been allocated
void copy_solution(const solution& a, solution& b, uint route_size)
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

// Saves sol to best_sol only if it has a better value
// If so, will also print information about the solution found
inline void update_best_solution(
    const solution& sol,
    solution& best_sol,
    uint sol_size,
    runtime_info& info)
{
    if (sol.value < best_sol.value) {
        copy_solution(sol, best_sol, sol_size);
        std::cout << '\n';
        print_time(info.timer);
        print_solution(sol, sol_size);
    }
}

// An implementation of Greedy Randomized Adaptive Search Procedure
// Uses alpha to limit the size of the RCL
// Runs until iterations_limit or info.time_limit is reached
// Returns the best solution found
// Other informations are saved in info
solution grasp(
    const problem& prob,
    long iterations_limit,
    double alpha,
    runtime_info& info,
    std::mt19937& rng)
{
    solution sol;
    sol.route = new uint[prob.num_temples];

    solution best_sol;
    best_sol.route = new uint[prob.num_temples];
    best_sol.value = std::numeric_limits<uint>::max();

    problem prob_tmp;
    prob_tmp.temples = new temple[prob.num_temples];
    prob_tmp.distances = new uint[prob.num_temples * prob.num_temples];

    candidate *candidates = new candidate[prob.num_temples];
    bool *prereq_forward = new bool[prob.num_temples] {0};
    uint *search_order = new uint[prob.num_temples - 1];
    std::iota(search_order, &search_order[prob.num_temples - 1], 0);

    long iterations_left = iterations_limit;

    while (iterations_left > 0 && !check_time_limit(info)) {
        // Building initial solution requires num_temples iterations
        if (iterations_left - prob.num_temples <= 0)
            break;

        // Update number of total iterations run
        info.total_iterations += prob.num_temples;
        iterations_left -= prob.num_temples;

        // Build greedy randomized initial solution
        copy_problem(prob, prob_tmp);
        if (!greedy_randomized(prob_tmp, sol, candidates, alpha, info, rng))
            break;

        if (info.grasp_iterations == 0) {
            info.initial_sol_value = sol.value;
            std::cout << "\nInitial solution:\n";
            print_solution(sol, prob.num_temples);
        }

        // Run local search
        iterations_left = local_search(sol, prob, prereq_forward, search_order,
                                       iterations_left, info, rng);

        update_best_solution(sol, best_sol, prob.num_temples, info);

        info.grasp_iterations++;
    }

    delete[] sol.route;
    delete[] prob_tmp.temples;
    delete[] prob_tmp.distances;
    delete[] candidates;
    delete[] prereq_forward;
    delete[] search_order;

    return best_sol;
}

// Generate csv lines for preliminary results of a problem
void time_pre_results(const problem& prob, long iterations, std::ofstream& csv)
{
    double alphas[5] = {0.05, 0.1, 0.2, 0.3, 0.5};

    std::mt19937 rng;
    runtime_info info;
    solution sol;

    for (double alpha : alphas) {
        rng.seed(1);

        info = runtime_info();

        sol = grasp(prob, iterations, alpha, info, rng);
        delete[] sol.route;

        csv << std::format("{},{},{},{},{},{},{},{}\n",
            5, alpha, 1, info.total_iterations, info.grasp_iterations,
            get_elapsed_time(info.timer), info.initial_sol_value, sol.value);
        csv.flush();
    }
}

// Generate csv lines for final results of a problem
// Limits the first run by time
// The others are limited by the number of iterations achieved on the first
void time_results(const problem& prob, double time, std::ofstream& csv)
{
    double alphas[2] = {0.05, 0.2};
    uint seeds[5] = {1, 2, 3, 4, 5};

    long iterations_limit = std::numeric_limits<long>::max();
    std::mt19937 rng;
    solution sol;
    runtime_info info;
    info.time_limit = time;

    uint line = 0;
    for (double alpha : alphas) {
        for (uint seed : seeds) {
            rng.seed(seed);

            sol = grasp(prob, iterations_limit, alpha, info, rng);
            delete[] sol.route;

            if (line == 0)
                iterations_limit = info.total_iterations;

            csv << std::format("{},{},{},{},{},{},{},{}\n",
                time, alpha, seed, info.total_iterations, info.grasp_iterations,
                get_elapsed_time(info.timer), info.initial_sol_value, sol.value);
            csv.flush();

            info = runtime_info();
            line++;
        }
    }
}

// Generate CSV files for each problem with its results
// Problem files should be named "{:02}.txt" (Ex: "07.txt")
void generate_results(std::string_view& instances_dir, bool pre = false, uint num_instances = 10)
{
    // Used for pre_results
    long iterations[10] = {
        73491534, 89026157, 32982444, 10489985, 54104004,
        75804993, 71246358, 88245156, 36808028, 50494627
    };

    for (uint i = 1; i <= num_instances; ++i) {
        std::string path = std::format("{}{:02}.txt", instances_dir, i);
        std::ifstream input(path);
        const problem prob = parse_input_file(input);
        input.close();

        path = std::format("{}{:02}.csv", instances_dir, i);
        std::ofstream csv(path);
        csv << "tempo_alvo,alpha,seed,iterações_parciais,iterações_grasp,tempo_execução,valor_inicial,valor\n";

        if (pre) {
            time_pre_results(prob, iterations[i - 1], csv);
        }
        else {
            time_results(prob, 5, csv);
            time_results(prob, 300, csv);
        }

        csv.close();
        delete[] prob.temples;
        delete[] prob.distances;
    }
}

int main(int argc, char *argv[])
{
    parameters params = parse_parameters(argc, argv);

    if (!params.generate_results && !params.generate_pre_results) {
        print_parameters(params);

        std::mt19937 rng(params.seed);

        std::ifstream input_file(params.input_path.data());
        const problem prob = parse_input_file(input_file);
        input_file.close();

        runtime_info info;
        info.time_limit = params.time_limit;

        solution best_sol = grasp(prob, params.iterations_limit, params.alpha, info, rng);

        delete[] best_sol.route;
        delete[] prob.temples;
        delete[] prob.distances;
    }
    else {
        generate_results(params.input_path, params.generate_pre_results);
    }

    return 0;
}
