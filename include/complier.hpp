// 只支持int/char[*]类型
// 将一个char作为内存最小单位 指针,int大小均为4
// 函数调用:
// 调用fun(int a,int b,...)
// caller中:
// 计算a
// lea&li a    stack: a
// 计算b
// lea&li b    stack: a   b
//...
// call addr<fun>
// 低地址 +----------------+
//       |     参数 N     |
//       |     参数...    |
//       |     参数 1     | -12
//       +----------------+
//       |  返回地址(pc+1) | -8
//       +----------------+
//       |   旧的BP值     |  -4
// 高地址 +----------------+<- 新的BP和SP都指向这里

// 低地址 +----------------+
//       |     参数 N     |
//       |     参数...    |
//       |     参数 1     | -12
//       +----------------+
//       |  返回地址(pc+1) | -8
//       +----------------+
//       |   旧的BP值     |  -4
//       +----------------+      bp
//       |   tpvar0       |  0
//       |   tpvar1       |  4
// 高地址 +----------------+<-  sp
// fun中: a=bp[-(n*4)] b=bp[-((n-1)*4)]... retaddr=bp[0] obp=bp[4]
// nargs n  分配n个参数
// ret ax携带返回值,jump bp[0]

// dargs n 弹出n个参数
// [可选] push ax->stack压回返回值

// 编译时的空间分配:
// 全局var:直接访问  IMM + 数 LI 访问
// funvar:bp+偏移   LEA + 数 LI 访问
// funvar初始stack大小为8,为 obp opc+1预留位置
#pragma once
#include "error.hpp"
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <peglib.h>
#include <stack>
#include <string>
#include <variant>
#include <vector>
#include <vm.h>
#include "obj.h"
#include "ComplierParser.h"
// AST 节点基类
struct ASM
{
    std::string content;
    enum class basic_asm
    {
        MOVE, // MOVE ax stack;ax值替换栈顶值
        IMM,  // 立即数入栈
        LEA,  // 将bp+arg推入栈顶
        LI,   // 栈顶为地址，替换栈顶为值
        LC,   // 栈顶为地址，替换栈顶为值
        SI,   // 栈顶为值，次栈顶为地址
        SC,   // 栈顶为值，次栈顶为地址
        ADD,  // 二元运算符汇编栈顶为右操作数，次栈顶为左操作数，出栈操作数，入栈结果
        SUB,
        MUL,
        DIV,
        MOD,
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
        UP, // 分配data段
        SYSTEMCALL
    };
    static std::string asm2string(basic_asm in)
    {
        static const std::unordered_map<basic_asm, std::string> asm2stringmap{
            {basic_asm::MOVE, "MOVE"}, {basic_asm::IMM, "IMM"},
            {basic_asm::LEA, "LEA"},   {basic_asm::LI, "LI"},
            {basic_asm::LC, "LC"},     {basic_asm::SI, "SI"},
            {basic_asm::SC, "SC"},     {basic_asm::ADD, "ADD"},
            {basic_asm::SUB, "SUB"},   {basic_asm::MUL, "MUL"},
            {basic_asm::DIV, "DIV"},   {basic_asm::MOD, "MOD"},
            {basic_asm::JMP, "JMP"},   {basic_asm::JZ, "JZ"},
            {basic_asm::JNZ, "JNZ"},   {basic_asm::PUSH, "PUSH"},
            {basic_asm::POP, "POP"},   {basic_asm::CALL, "CALL"},
            {basic_asm::NVAR, "NVAR"}, {basic_asm::RET, "RET"},
            {basic_asm::EXIT, "EXIT"}, {basic_asm::DARG, "DARG"},
            {basic_asm::UP, "UP"},     {basic_asm::SYSTEMCALL, "SYSTEMCALL"}};
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
struct exefile
{
    size_t global_size = 0;
    std::vector<std::string> asms;
};
struct linker
{
    std::unordered_map<std::string, size_t> addrmap;
    std::vector<OBJ>& objs;
    exefile exe;
    std::expected<size_t, error> pushfunc(std::string funcname);
    std::expected<std::vector<std::string>, error> process();
    linker(std::vector<OBJ>& ins) : objs(ins)
    {
    }
};
struct complier
{
    static std::expected<std::vector<std::string>, error> process(std::vector<std::string> paths);
};