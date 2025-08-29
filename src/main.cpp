#include "complier.hpp"
#include "vm.h"
#include <iostream>
#include <peglib.h>
int main()
{
    auto ret = complier::process({"../test/test1.c"}).value();
    // for (int i = 0; i < ret.size(); i++)
    // {
    //     std::cout << "[" << i << "]: " << ret[i] << '\n';
    // }
    // VM vm{ret};
    // if (auto ret = vm.run())
    // {
    //     std::cout << "main returns: " << ret.value() << '\n';
    // }
    // else
    // {
    //     std::cout << "failed!\n";
    // }
    return 0;
}