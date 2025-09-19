#include "complier.hpp"
#include <iostream>

int main(int argc, const char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);
    auto ret = complier::process(args);
    if (!ret)
    {
        std::cout << "error\n";
    }
    else
    {
        for (int i = 0; i < ret.value().size(); i++)
        {
            std::cout << '[' << i << "]:" << ret.value()[i] << '\n';
        }
    }
    return 0;
}