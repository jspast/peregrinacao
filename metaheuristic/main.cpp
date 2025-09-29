#include <cmath>
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

int temples_distance(struct temple a, struct temple b)
{
    return std::sqrt((float)((b.x - a.x)*(b.x - a.x) + (b.y - a.y)*(b.y - a.y))) * 100;
}

void print_solution()
{

}

void print_help()
{
    std::cout << "GRASPer peregrinação solver using GRASP\n";
    std::cout << "usage: grasper [file] [max_iterations] [seed]\n";
}

int main(int argc, char *argv[])
{
    std::string_view input_path;
    uint max_iterations;
    uint seed;

    if (argc < 4) {
        print_help();
        return 0;
    }

    input_path = argv[1];
    max_iterations = std::atoi(argv[2]);
    seed = std::atoi(argv[3]);

    return 0;
}
