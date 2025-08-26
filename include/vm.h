#pragma once
#include "error.hpp"
#include <expected>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>
struct VCPU
{
    enum State
    {
        OK,
        STOP,
        ERROR
    };
    State state = State::OK;
    std::vector<std::string> assembly_code; // 直接存储汇编指令字符串
    std::vector<char> stack;                // memery

    int pc; // program counter (现在是assembly_code的索引)
    int sp; // stack pointer
    int bp; // base pointer
    int ax; // accumulator register
    int cycle;

    // 构造函数
    VCPU(int stacksize = 1024) : pc(0), sp(0), bp(0), ax(0), cycle(0)
    {
        stack.resize(stacksize); // 默认memery大小
    }
    inline static const constexpr size_t size_word = 4; // 32位cpu
    // [TODO] 安全检查
    bool stackpop()
    {
        sp -= size_word;
        return true;
    }
    void stackpush(int in)
    {
        sp += size_word;
        *reinterpret_cast<int*>(&stack[sp - 4]) = in;
    }
    int stacktop()
    {
        return *reinterpret_cast<int*>(&stack[sp - 4]);
    }
    int memget(size_t addr, size_t bytesize = 4)
    {
        if (bytesize == 1)
        {
            return stack[addr];
        }
        else if (bytesize == 4)
        {
            return *reinterpret_cast<int*>(&stack[addr]);
        }
        else
        {
            throw("unsurpport data length");
            // unsurpport
        }
    }
    bool memloal(size_t addr, int value, size_t bytesize = 4)
    {
        if (bytesize == 1)
        {
            stack[addr] = value;
        }
        else if (bytesize == 4)
        {
            *reinterpret_cast<int*>(&stack[addr]) = value;
        }
        else
        {
            // unsurpport
        }
        return true;
    }
    void do_ins(std::string thisasm);
    void DOMOVE(std::string thisasm);
    void DOIMM(std::string thisasm);
    void DOLEA(std::string thisasm);
    void DOLI(std::string thisasm);
    void DOLC(std::string thisasm);
    void DOSI(std::string thisasm);
    void DOSC(std::string thisasm);
    void DOADD(std::string thisasm);
    void DOSUB(std::string thisasm);
    void DOMUL(std::string thisasm);
    void DODIV(std::string thisasm);
    void DOMOD(std::string thisasm);
    void DOJMP(std::string thisasm);
    void DOJZ(std::string thisasm);
    void DOJNZ(std::string thisasm);
    void DOPUSH(std::string thisasm); // ax->stack
    void DOPOP(std::string thisasm);  // stack->ax
    void DOCALL(std::string thisasm); // 栈顶为地址
    void DONVAR(std::string thisasm); // 分配函数局部变量栈空间,4字节为单位
    void DORET(std::string thisasm);
    void DOEXIT(std::string thisasm);
    void DODARG(std::string thisasm);
    void DOUP(std::string thisasm); // 分配data段
    void DOSYSTEMCALL(std::string thisasm);
    bool print()
    {
        std::cout << "指令: " << assembly_code[pc] << '\n';
        std::cout << "ax: " << ax << '\n';
        std::cout << "sp: " << sp << '\n';
        std::cout << "bp: " << bp << '\n';
        std::cout << "pc: " << pc << '\n';
        for (int i = 0; i < sp; i += 4)
        {
            std::cout << "[" << i << "]: " << *reinterpret_cast<int*>(&stack[i]) << '\n';
        }
        std::cout << '\n';
        return true;
    }
    void run()
    {
        do
        {
            print();
            do_ins(assembly_code[pc]);
            // [exit/error判断]
            cycle++;
            pc++;
        } while (state == State::OK);
    }
};

struct VM
{
    // std::ostream* logout = nullptr;
    VCPU cpu;
    // std::expected<bool, error> print();
    std::expected<bool, error> setlogpath(std::string path);
    std::expected<bool, error> eachcycle();
    std::expected<int, error> run();
    bool enable_debug = true;
    VM(std::string path);
    VM(std::vector<std::string> asms)
    {
        cpu.assembly_code = asms;
    }
};
