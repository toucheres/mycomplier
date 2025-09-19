#pragma once
#include <filesystem>
#include <format>
#include <iostream>
#include <regex>
#include <unordered_map>
struct ASM
{
    std::string content;
    enum class basic_asm
    {
        MOVE, // MOVE ax stack;ax值替换栈顶值
        COPY, // 栈顶复制一份到栈顶
        IMM,  // 立即数入栈
        LEA,  // 将bp+arg推入栈顶
        LI,   // 栈顶为地址，替换栈顶为值
        LC,   // 栈顶为地址，替换栈顶为值
        LW,   // 栈顶为地址，替换栈顶为值
        SI,   // 栈顶为值，次栈顶为地址
        SC,   // 栈顶为值，次栈顶为地址
        SW,   // 栈顶为值，次栈顶为地址
        ADD,  // 二元运算符汇编栈顶为右操作数，次栈顶为左操作数，出栈操作数，入栈结果
        SUB,
        MUL,
        DIV,
        MOD,
        AND,
        OR,
        LSHIFT,
        RSHIFT,
        XOR,
        SMALL,  // <
        BIG,    // >
        SMALLE, // <=
        BIGE,   // >=
        CMP,    // 相等为1,不相等为0
        CMPN,   // CMP取反
        NOT,
        JMP,
        JZ,
        JNZ,
        PUSH, // ax->stack
        POP,  // stack->ax
        CALL, // 栈顶为地址
        NVAR, // 分配函数局部变量栈空间,4字节为单位
        RET,
        EXIT,
        DARG,
        SYSTEMCALL
    };
    static std::string asm2string(basic_asm in)
    {
        static const std::unordered_map<basic_asm, std::string> asm2stringmap{
            {basic_asm::MOVE, "MOVE"},
            {basic_asm::IMM, "IMM"},
            {basic_asm::LEA, "LEA"},
            {basic_asm::LI, "LI"},
            {basic_asm::LC, "LC"},
            {basic_asm::SI, "SI"},
            {basic_asm::SC, "SC"},
            {basic_asm::ADD, "ADD"},
            {basic_asm::SUB, "SUB"},
            {basic_asm::MUL, "MUL"},
            {basic_asm::DIV, "DIV"},
            {basic_asm::MOD, "MOD"},
            {basic_asm::JMP, "JMP"},
            {basic_asm::JZ, "JZ"},
            {basic_asm::JNZ, "JNZ"},
            {basic_asm::PUSH, "PUSH"},
            {basic_asm::POP, "POP"},
            {basic_asm::CALL, "CALL"},
            {basic_asm::NVAR, "NVAR"},
            {basic_asm::RET, "RET"},
            {basic_asm::EXIT, "EXIT"},
            {basic_asm::DARG, "DARG"},
            {basic_asm::COPY, "COPY"},
            {basic_asm::SYSTEMCALL, "SYSTEMCALL"}};
        auto it = asm2stringmap.find(in);
        if (it != asm2stringmap.end())
            return it->second;
        else
            return "UNKNOWN";
    }
    enum class SYSTEMCALL_Type
    {
    };
    ASM(basic_asm basm, auto&&... args)
    {
        static auto tostr = [](auto&& in)
        {
            if constexpr (requires { std::to_string(in); })
            {
                return std::to_string(in);
            }
            else if constexpr (requires { std::string{in}; })
            {
                return std::string{in};
            }
            throw;
        };

        content = asm2string(basm);

        // 使用折叠表达式处理所有参数
        if constexpr (sizeof...(args) > 0)
        {
            // 为每个参数添加空格和字符串表示
            ((content += " " + tostr(std::forward<decltype(args)>(args))), ...);
        }
    }
    ASM(std::string in) : content(in)
    {
    }
    operator std::string()
    {
        return content;
    }
};