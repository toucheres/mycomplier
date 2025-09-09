#include "complier.hpp"
#include <iostream>

int main(int argc, const char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);
    complier::process(args);
    return 0;
}