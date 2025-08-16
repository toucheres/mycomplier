#pragma once
#include "enums.h"
#include "error.hpp"
#include <expected>
#include <stack>
#include <string>
#include <tokenprocessor.h>
#include <vector>
#include <vm.h>
#include <map>
// 只支持int[*]类型
// 将一个int作为内存最小单位,指针,int大小均为1
// 函数调用:
// 调用fun(int a,int b,...)
// caller中:
// 计算a
// push a    stack: a
// 计算b
// push b    stack: a   b
//...
// call addr<fun> 压入pc+1 压入bp bp=sp jump-addr<fun>  stack: a   b  ...  opc+1  obp
//                                                                          bp

// fun中: a=bp[-n] b=bp[-(n-1)]... retaddr=bp[0] obp=bp[1]
// ret ax携带返回值,jump bp[0]

// nargs n  弹出n个参数
// [可选] ax->stack

// 编译时的空间分配:
// 全局var:直接访问  LEAG + 数 访问
// funvar:bp+偏移   LEA + 数 访问
// funvar初始stack为1,为opc+1预留位置
struct id_def
{
    int addr;
    std::string id;
    Type type;
    bool defined = false;
};

struct var_def : id_def
{
    int defult_value = 0;
};

struct fun_def : id_def
{
    std::vector<Type> argtypes;
    std::vector<var_def> vars;
};
struct fun_defs
{
  public:
  private:
    std::vector<fun_def> fun_defines;

  public:
    std::expected<fun_def, error> find(const std::string& id);
    std::expected<bool, error> push(fun_def fun_def);
};
struct var_defs
{
    // 仿照stack处理不同作用域变量生命周期
  private:
    struct eachnamespace
    {
        std::vector<var_def> var_defines_namespace;
        std::expected<var_def*, error> find(const std::string& id);
        std::expected<bool, error> push(
            var_def var_def); // 在push中处理重定义: 每层namespace变量声明只能一次
    };
    std::vector<eachnamespace> namespace_defines;
    int dy_stack_size = 0;
    int max_stack_size = 0;
    int old_stack_size = 0;

  public:
    // 构造函数，初始化时创建全局作用域
    var_defs()
    {
        namespace_defines.emplace_back(); // 创建全局作用域（第0层）
    }

    std::expected<var_def*, error> find(const std::string& id);
    std::expected<bool, error> push(var_def var_def);
    // std::expected<bool, error> push_func_args(var_def var_def);
    std::expected<bool, error> push_func_args(std::vector<var_def> var_def);
    void into_new_namespace();
    int get_max_size();
    void outto_old_namespace();
    void clear();
};
struct obj
{
    var_defs global_var_defs_; // 统一的变量定义容器，包含全局变量
    var_defs func_var_defs_;   // 统一的变量定义容器，包含函数局部变量关于函数起始的偏移
    fun_defs fun_defs_;
    std::vector<std::string> content; // 改为vector格式，便于调试

    // 源在前，目标在后

    void pushASM(VM::ASM ASM);
    void pushASM(VM::ASM ASM, int arg);
    void pushASM(VM::ASM ASM, int src, int obj);

    // 获取vector格式的汇编代码（现在直接返回content）
    const std::vector<std::string>& get_assembly_vector() const;
};
class Complier
{
    static std::expected<bool, error> try_parse_fun(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_block(Tokens& tokens, obj& obj);
    static std::expected<std::vector<Type>, error> try_parse_args(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_assignment_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_logical_or_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_logical_and_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_equality_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_relational_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_additive_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_multiplicative_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_unary_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_postfix_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_primary(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_while(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_if(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_return(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_global_var(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_func_var(Tokens& tokens, obj& obj);

    // 辅助函数
    static bool is_binary_operator(const std::string& token);
    static bool is_assignment_operator(const std::string& token);
    static bool is_relational_operator(const std::string& token);
    static bool is_multiplicative_operator(const std::string& token);
    static bool is_unary_operator(const std::string& token);
    static bool is_number(const std::string& token);
    static void generate_binary_op_asm(const std::string& op, obj& obj);

  public:
    std::expected<obj, error> process(std::vector<std::string> args);
    std::expected<obj, error> eachFile(std::string path);
};