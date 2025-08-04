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

    // 使用新的vector接口
    auto assembly_vector = ret.value().get_assembly_vector();
    
    // 输出汇编代码
    std::cout << "Generated Assembly:" << std::endl;
    for (size_t i = 0; i < assembly_vector.size(); ++i) {
        std::cout << "[" << i << "] " << assembly_vector[i] << std::endl;
    }
    std::cout << std::endl;
    
    VM vm;
    
    // 启用调试功能
    vm.enable_debug("vm_execution.log");
    
    vm.load_assembly_vector(assembly_vector);
    std::cout << "ret: " << vm.start() << '\n';
    
    // 关闭调试功能
    vm.disable_debug();
    
    std::cout << "Debug log saved to: vm_execution.log" << std::endl;
    
    return 0;
}