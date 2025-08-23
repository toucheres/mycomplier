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
// call addr<fun> 压入pc+1 压入bp bp=sp jump-addr<fun>  stack: a   b  ...  opc+1  obp
//                                                                          bp

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
// AST 节点基类
struct ASM
{
    std::string content;
    enum class basic_asm
    {
        MOVE, // MOVE ax stack;ax值替换栈顶值
        IMM,  // 立即数
        LEA,
        LI,
        LC,
        SI,
        SC,
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
        CALL,
        JMP,
        JZ,
        JNZ,
        PUSH, // ax->stack
        POP,  // stack->ax
        NARG,
        RET,
        DARG,
        SYSTEMCALL
    };
    enum class SYSTEMCALL_Type
    {
    };
    ASM(basic_asm basm, auto&&... args)
    {
    }
    ASM(std::string in) : content(in)
    {
    }
    operator std::string()
    {
        return content;
    }
};

// struct ASTNode
// {
//     virtual ~ASTNode() = default;
//     // 代码生成方法
//     virtual void generate(std::vector<std::string>& code) const = 0;
// };

// 类型表示
struct Type
{
    enum class BasicType
    {
        Int,
        Char,
        Void
    };
    BasicType basic_type;
    int pointer_level = 0; // 指针层级

    bool is_pointer() const
    {
        return pointer_level > 0;
    }
    std::string to_string() const;
    size_t getsize() const;
    Type(std::shared_ptr<peg::Ast> astnode);
    Type() = default;
};
struct Identifi
{
    std::string name;
    Type type;
    bool is_defined;
    int addr;
};
struct varDef : Identifi
{
    varDef(std::shared_ptr<peg::Ast> astnode);
    size_t get_addr_in_mem(size_t posnow);
    varDef() = default;
};
// struct argDef
// {
//     std::string name;
//     Type type;
//     argDef(std::shared_ptr<peg::Ast> astnode);
// };
struct funcDef : Identifi
{
    std::vector<std::string> asms;
    std::vector<varDef> args;
    std::vector<std::vector<varDef>> funcvar_stack;
    size_t max_stack_size = VCPU::size_word * 2;
    size_t stack_size_now = VCPU::size_word * 2;
    void enter_scope();
    void exit_scope();
    const varDef* lookup_var(const std::string& name) const;
    const varDef* add_var(const varDef& vardef);
    bool add_arg(std::vector<varDef>& vardef);
    funcDef() = default;
};

// 符号表
class SymbolTable
{
  private:
    std::unordered_map<std::string, varDef> globalvar;
    std::unordered_map<std::string, funcDef> globalfuncdef;
    int globalvarsize;

  public:
    SymbolTable() = default;
    // 全局变量与函数定义
    varDef* add_global_symbol(const varDef& vardef);
    funcDef* add_global_symbol(const funcDef& funcdef);
    // 查找
    varDef* lookup_var(const std::string& name);
    funcDef* lookup_fun(const std::string& name);
};

struct OBJ
{
    std::string name;
    // AST 根节点
    std::shared_ptr<peg::Ast> program;
    // 符号表
    SymbolTable symbol_table;
    // 全局偏移
    int bias = 0;
    // 代码生成接口
    std::expected<bool, error> generate_code();
    std::expected<bool, error> generate_expression(std::shared_ptr<peg::Ast> expr,
                                                        funcDef* func);
    // [TODO] deep替换为外层向内层传递信息
    std::expected<bool, error> generate_code(std::shared_ptr<peg::Ast> astnode, funcDef* funname,
                                             size_t deep);
    OBJ(std::shared_ptr<peg::Ast> root, std::string inname) : program(root), name(inname)
    {
        funcDef __global_init_fun{};
        __global_init_fun.name = "__global_init_" + inname;
        __global_init_fun.is_defined = true;
        symbol_table.add_global_symbol(__global_init_fun);
    }
};
struct linker
{
    static std::expected<std::vector<std::string>, error> process(std::vector<OBJ>& objs);
};
struct complier
{
    static std::expected<std::vector<std::string>, error> process(std::vector<std::string> paths);
};