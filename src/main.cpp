#include "complier.hpp"
#include "vm.h"
#include <iostream>
// [TODO] ptr的自增
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
            std::cout << "[" << i << "]:" << ret.value()[i] << '\n';
        }
    }
    VM vm{ret.value()};
    vm.enable_debug = false;
    auto retval = vm.run();
    if (retval)
    {
        std::cout << "ret: " << retval.value() << '\n';
    }
    else
    {
        std::cout << "error\n";
    }
    return 0;
}