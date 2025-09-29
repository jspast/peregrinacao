#include <cmath>
#include <fstream>
#include <iostream>
#include <string_view>

using uint = unsigned int;

struct temple {
    uint x;
    uint y;
};

struct dependency {
    uint a;
    uint b;
};

struct problem {
    uint num_temples;
    struct temple *temples;
    uint num_dependencies;
    struct dependency *dependencies;
};

void print_help()
{
    std::cout << "GRASPer peregrinação solver using GRASP\n";
    std::cout << "usage: grasper [file] [max_iterations] [seed]\n";
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

    file >> prob->num_dependencies;
    prob->dependencies = new struct dependency[prob->num_dependencies];
    for (uint i = 0; i < prob->num_dependencies; ++i)
        file >> prob->dependencies[i].a >> prob->dependencies[i].b;

    return *prob;
}

int temples_distance(const struct temple& a, const struct temple& b)
{
    return std::sqrt((float)((b.x - a.x)*(b.x - a.x) + (b.y - a.y)*(b.y - a.y))) * 100;
}

void print_solution()
{

}

int main(int argc, char *argv[])
{
    std::string_view input_path;
    uint max_iterations;
    uint seed;

    if (argc < 4) {
        print_help();
        std::cerr << "\nError: insufficient number of parameters\n";
        std::exit(EXIT_FAILURE);
    }

    input_path = argv[1];
    max_iterations = std::atoi(argv[2]);
    seed = std::atoi(argv[3]);

    const struct problem prob = parse_input(std::ifstream(input_path.data()));

    return 0;
}
