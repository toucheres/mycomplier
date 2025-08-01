#pragma once
#include "enums.h"
#include "error.hpp"
#include <expected>
#include <stack>
#include <string>
#include <vector>
#include <vm.h>
// GLOBAL = var_decl | var_def | fun_decl | fun_def
// type = int/char [*]
// var_decl = type id;
// fun_decl = type id([type id,]);
// fun_def = type id([type id,]){statements};

struct id_def
{
    size_t addr;
    std::string id;
    Type type;
    bool defined = false;
};

struct var_def : id_def
{
    int defult_value = 0;
};
struct fun_defs
{
  public:
    enum class error
    {
    };

  private:
    std::vector<fun_def> fun_defines;

  public:
    std::expected<fun_defs, error> find(const std::string& id);
    std::expected<bool, error> push(fun_def dun_def);
};
struct fun_def : id_def
{
    std::vector<Basic_Type> argtypes;
    std::vector<var_def> vars;
};
struct var_defs
{
    // 仿照stack处理不同作用域生命
  private:
    struct eachnamespace
    {
        std::vector<var_def> var_defines_namespace;
        std::expected<var_def&, error> find(const std::string& id);
        std::expected<bool, error> push(
            var_def var_def); // 在push中处理重定义: 每层namespace变量声明只能一次
    };
    std::vector<var_def> var_defines;
    std::vector<eachnamespace> namespace_defines;

  public:
    std::expected<var_def&, error> find(const std::string& id);
    std::expected<bool, error> push(var_def var_def);
    std::expected<bool, error> push_arg(var_def var_def);
    void into_new_namespace();
    void out_new_namespace();
};
struct obj
{
    var_defs global_var_defs;
    var_defs fun_var_defs;
    fun_defs fun_defs;
    std::stack<std::string> content;
    // 源在前，目标在后
    void pushASM(VM::ASM ASM);
    void pushASM(VM::ASM ASM, int arg);
    void pushASM(VM::ASM ASM, int src, int obj);
};
class Complier
{
    static std::expected<bool, error> try_parse_fun(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_block(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_args(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_expr(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_while(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_if(Tokens& tokens, obj& obj);
    static std::expected<bool, error> try_parse_var(Tokens& tokens, obj& obj);

  public:
    int process(std::vector<std::string> args);
    std::expected<obj, error> eachFile(std::string path);
};