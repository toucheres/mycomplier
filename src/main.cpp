#include "complier.hpp"
#include "vm.h"
#include <iostream>

int main()
{
    Complier com;
    auto ret = com.process({"/home/toucher/vscoderope/mycomplier/test/test1.c"});

    if (!ret)
    {
        std::cerr << "编译失败" << std::endl;
        return -1;
    }

    auto asms = ret.value().content;
    VM vm;
    vm.load_assembly_stack(asms);
    std::cout << vm.start() << '\n';
    return 0;
}