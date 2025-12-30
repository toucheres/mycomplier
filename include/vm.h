#pragma once
#include "error.hpp"
#include <array>
#include <expected>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>
class VM;
template <size_t StackSize = 1024000, class Word = int64_t> struct VCPU
{
    inline static const size_t size_word = sizeof(Word);
    std::map<int, std::function<void(VCPU<>&)>> systemcall_table;
    alignas(8) std::vector<std::string> asms;
    std::array<Word, StackSize / size_word> mem{0};
    Word axmem = 0;
    Word* ax = &axmem;
    Word* bp = &mem.back();
    Word* sp = &mem.back();
    Word* ss = &mem.back();
    Word* ds = &mem.front();
    size_t ip = 0;
    enum class CpuState
    {
        OK,
        ERROR,
        OVER
    };
    CpuState state = CpuState::OK;
    void do_ins(const std::string& in);
    void step();
    void run();
};
struct VM
{
    enum systemcall
    {
        WRITE,
        MALLOC,
        FREE,
        BREAKPOINT
    };
    VM(const std::vector<std::string>& asms);
    bool enable_debug = true;
    VCPU<> vcpu;
    std::optional<int64_t> run();
    void debug();
};

template <size_t StackSize, class Word>
inline void VCPU<StackSize, Word>::do_ins(const std::string& in)
{
    // [TODO] call ret systemcall
    std::stringstream str(in);
    std::string ins;
    str >> ins;
    if (ins == "POP")
    {
        *ax = *sp;
        // pop: move toward higher address
        sp++;
        return;
    }
    else if (ins == "PUSH")
    {
        // push: move toward lower address
        sp--;
        *sp = *ax;
        return;
    }
    else if (ins == "IMM")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        sp--;
        *sp = static_cast<Word>(num);
        return;
    }
    else if (ins == "LEA")
    {
        *sp = reinterpret_cast<Word>(reinterpret_cast<char*>(bp) + *sp);
        return;
    }
    else if (ins == "LEAD")
    {
        *sp = reinterpret_cast<Word>(reinterpret_cast<char*>(ds) + *sp);
        return;
    }
    else if (ins == "COPY")
    {
        // duplicate top: new top at lower address equals previous top
        sp--;
        *sp = *(sp + 1);
        return;
    }
    else if (ins == "LI")
    {
        int32_t* addr = reinterpret_cast<int32_t*>(*sp);
        *sp = *addr;
        return;
    }
    else if (ins == "LC")
    {
        char* addr = reinterpret_cast<char*>(*sp);
        *sp = *addr;
        return;
    }
    else if (ins == "LW")
    {
        int64_t* addr = reinterpret_cast<int64_t*>(*sp);
        *sp = *addr;
        return;
    }
    else if (ins == "SI")
    {
        auto val = *sp;
        sp++;
        int32_t* addr = reinterpret_cast<int32_t*>(*sp);
        sp++;
        *addr = val;
        return;
    }
    else if (ins == "SC")
    {
        auto val = *sp;
        sp++;
        char* addr = reinterpret_cast<char*>(*sp);
        sp++;
        *addr = val;
        return;
    }
    else if (ins == "SW")
    {
        auto val = *sp;
        sp++;
        int64_t* addr = reinterpret_cast<int64_t*>(*sp);
        sp++;
        *addr = val;
        return;
    }
    else if (ins == "ADD")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left + right;
        return;
    }
    else if (ins == "SUB")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left - right;
        return;
    }
    else if (ins == "MUL")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left * right;
        return;
    }
    else if (ins == "DIV")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left / right;
        return;
    }
    else if (ins == "MOD")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left % right;
        return;
    }
    else if (ins == "AND")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left & right;
        return;
    }
    else if (ins == "OR")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left | right;
        return;
    }
    else if (ins == "LSHIFT")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left << right;
        return;
    }
    else if (ins == "RSHIFT")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left >> right;
        return;
    }
    else if (ins == "XOR")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left ^ right;
        return;
    }
    else if (ins == "SMALL")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left < right;
        return;
    }
    else if (ins == "BIG")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left > right;
        return;
    }
    else if (ins == "SMALLE")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left <= right;
        return;
    }
    else if (ins == "BIGE")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left >= right;
        return;
    }
    else if (ins == "CMP")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left == right;
        return;
    }
    else if (ins == "CMPN")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = !(left == right);
        return;
    }
    else if (ins == "NOT")
    {
        auto right = *sp;
        *sp = !right;
        return;
    }
    else if (ins == "JMP")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        ip = num - 1; // 抵消ip自增
        return;
    }
    else if (ins == "JZ")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        auto right = *sp;
        sp++;
        if (!right)
        {
            ip = num - 1; // 抵消ip自增
        }
        return;
    }
    else if (ins == "JNZ")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        auto right = *sp;
        sp++;
        if (right)
        {
            ip = num - 1; // 抵消ip自增
        }
        return;
    }
    else if (ins == "CALL")
    {
        // 约定：栈顶为目标地址，栈向低地址增长
        // CALL：pop 目标地址；push 返回地址；push 旧bp；bp=sp；跳转
        auto dest = static_cast<size_t>(*sp);
        sp++; // pop target

        // push return address
        sp--;
        *sp = static_cast<Word>(ip + 1);

        // push old bp
        sp--;
        *sp = reinterpret_cast<Word>(bp);

        // new frame
        // bp = reinterpret_cast<Word*>(*sp);
        bp = sp;

        ip = dest - 1; // 跳转（抵消 step 自增）
        return;
    }
    else if (ins == "RET")
    {
        // 约定：RET 时，ax为函数返回值
        *ax = *sp;
        sp++; // pop return value

        // 丢弃局部变量：sp 回到当前帧基址（旧bp 存在 [bp]）
        sp = bp;

        // 弹出旧bp（位于 [sp]）
        Word oldbp_val = *sp;
        sp++;
        // 弹出返回地址（位于 [sp]）
        Word retaddr = *sp;
        sp++;

        // 恢复 bp
        bp = reinterpret_cast<Word*>(oldbp_val);

        // // 把返回值压回给调用者
        // sp--;
        // *sp = retv;

        ip = static_cast<size_t>(retaddr) - 1;
        return;
    }
    else if (ins == "NVAR")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        while (num--)
        {
            sp--;
        }
        return;
    }
    else if (ins == "DARG")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        while (num--)
        {
            sp++;
        }
        return;
    }
    else if (ins == "EXIT")
    {
        state = VCPU::CpuState::OVER;
        return;
    }
    else if (ins == "SYSTEMCALL") // systemcall由调用者(用户)清理参数
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        systemcall_table[num](*this);
        return;
    }
}

template <size_t StackSize, class Word> inline void VCPU<StackSize, Word>::step()
{
    do_ins(asms[ip]);
    ip++;
}

template <size_t StackSize, class Word> inline void VCPU<StackSize, Word>::run()
{
    while (state == CpuState::OK)
    {
        step();
    }
}
