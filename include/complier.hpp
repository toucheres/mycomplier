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
        CALL,
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
    Type(std::shared_ptr<peg::Ast> astnode);
    Type() = default;
};


// 符号表条目
struct SymbolEntry
{
    std::string name;
    Type type;
    int address;
    bool is_function;
    bool is_defined; // 声明vs定义
};

// 符号表
class SymbolTable
{
  private:
    std::vector<std::unordered_map<std::string, SymbolEntry>> scopes;
    int current_global_address = 0;
    int current_local_address = 0;

  public:
    SymbolTable()
    {
        scopes.emplace_back();
    } // 全局作用域

    void enter_scope();
    void exit_scope();

    bool add_symbol(const std::string& name, const Type& type, bool is_function, bool is_defined);
    std::optional<SymbolEntry> lookup(const std::string& name) const;
    std::optional<SymbolEntry> lookup_in_current_scope(const std::string& name) const;

    int allocate_global();
    int allocate_local();
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
};
struct argDef
{
    std::string name;
    Type type;
    argDef(std::shared_ptr<peg::Ast> astnode);
};
struct funcDef : Identifi
{
    std::vector<std::string> asms;
    std::vector<argDef> args;
    funcDef() = default;
};

struct OBJ
{
    // AST 根节点
    std::shared_ptr<peg::Ast> program;
    // 符号表
    SymbolTable symbol_table;
    // 汇编代码
    std::unordered_map<std::string, funcDef> funname_codes;
    // 位置追踪
    int pos = 0;
    // 代码生成接口
    std::expected<bool, error> generate_code();
    std::expected<bool, error> generate_code(std::shared_ptr<peg::Ast> astnode, std::string funname,
                                             size_t deep);
    OBJ(std::shared_ptr<peg::Ast> root) : program(root)
    {
        funname_codes.emplace("__global_init", funcDef{});
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