#include "complier.hpp"
#include "vm.h"
#include <iostream>
//[OK] [TODO] "xxx" 初始化char*和char[n]
// [TODO][BUG] "+=" 不可用
int main(int argc, const char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);
    auto ret = complier::process(args, true, 4);
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
        std::cout << "ret: " << retval.value() << " = " << std::hex << retval.value() << '\n';
    }
    else
    {
        std::cout << "error\n";
    }
    return 0;
}