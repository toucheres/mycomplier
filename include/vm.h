#pragma once
#include <fstream>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

struct VCPU
{
    std::vector<std::string> assembly_code; // 直接存储汇编指令字符串
    std::vector<char> stack;                // memery

    int pc; // program counter (现在是assembly_code的索引)
    int sp; // stack pointer
    int bp; // base pointer
    int ax; // accumulator register
    int cycle;

    // 构造函数
    VCPU() : pc(0), sp(0), bp(0), ax(0), cycle(0)
    {
        stack.resize(1024); // 默认memery大小
    }
};

struct VM
{
    VCPU cpu;
    VM(std::string path);
    VM(std::vector<std::string> asms);
};
