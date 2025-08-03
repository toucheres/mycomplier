#pragma once
#include "vm.h"
#include <string>
#include <vector>

class AssemblyRunner {
public:
    // 从字符串输出执行汇编代码
    static void run_from_output(const std::string& assembly_output);
    
    // 运行简单的测试用例
    static void run_simple_test();
};
