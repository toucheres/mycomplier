#include "vm.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>

class AssemblyRunner {
public:
    static void run_from_output(const std::string& assembly_output) {
        std::vector<std::string> lines;
        std::stringstream ss(assembly_output);
        std::string line;
        
        while (std::getline(ss, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        
        VM vm;
        vm.load_assembly_string(lines);
        
        std::cout << "=== 执行字符串格式汇编 ===" << std::endl;
        std::cout << "汇编代码:" << std::endl;
        for (const auto& l : lines) {
            std::cout << l << std::endl;
        }
        std::cout << std::endl;
        
        std::cout << "=== 开始执行 ===" << std::endl;
        int result = vm.start();
        
        std::cout << "=== 执行完成 ===" << std::endl;
        std::cout << "退出码: " << result << std::endl;
        
        vm.dump_registers();
    }
    
    static void run_simple_test() {
        std::vector<std::string> test_code = {
            "LEA", "0",      // 函数入口标签
            "IMM", "100",    // 压入立即数 100
            "SI", "8",       // 存储到地址 8 (变量 c)
            "LI", "0",       // 加载地址 0 的值 (变量 a)
            "LI", "4",       // 加载地址 4 的值 (变量 b)
            "IMM", "2",      // 压入立即数 2
            "MUL",           // 乘法 b * 2
            "ADD",           // 加法 a + (b * 2)
            "LI", "8",       // 加载地址 8 的值 (变量 c)
            "ADD",           // 加法 (a + b*2) + c
            "RET"            // 返回
        };
        
        VM vm;
        vm.load_assembly_string(test_code);
        
        // 初始化一些测试数据
        vm.cpu.data[0] = 10;  // a = 10
        *reinterpret_cast<int*>(&vm.cpu.data[0]) = 10;
        *reinterpret_cast<int*>(&vm.cpu.data[4]) = 20;  // b = 20
        
        std::cout << "=== 简单测试 ===" << std::endl;
        std::cout << "初始数据: a=10, b=20" << std::endl;
        std::cout << "预期结果: 10 + 20*2 + 100 = 160" << std::endl;
        
        int result = vm.start();
        std::cout << "执行结果: " << result << std::endl;
        vm.dump_registers();
        vm.dump_stack(5);
    }
};
