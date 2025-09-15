#pragma once
#include "ComplierBaseVisitor.h"
#include "ComplierVisitor.h"
#include <copyed_ptr.hpp>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <peglib.h>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <vm.h>
// 前向声明
struct Type;
struct varDef;

struct Type
{
    enum class BasicType
    {
        Int,
        Char,
        Void,
        Float,
        Double,
        Long,
        Short,
        Unsigned,
        Signed
    };
    enum class Kind
    {
        Undefined, // 初始
        ID,        // id
        Basic,     // 基本类型
        Pointer,   // 指针类型
        Array,     // 数组类型
        Function   // 函数类型
    };
    Type() = default;
    Type(const Type&) = default;
    Type(Kind kind, int arg);
    Type(Kind kind, std::string arg);        // for id
    Type(Kind kind, std::vector<Type> args); // for function
    Kind kind = Kind::Undefined;
    std::string id;
    // 基础类型
    BasicType basic_type; // avilable when kind == Basic
    copyed_ptr<Type> subType;
    std::vector<Type> args;
    int arr_or_ptr_num = -1;
    Type& getTop();
    bool pushTop(const Type& what);
    std::string to_string() const;
    size_t getsize() const;
};
struct Identifi
{
    std::string name;
    Type type;
    bool is_defined;
    int addr;
};
// [TODO] 对函数指针和数组的支持
struct varDef : Identifi
{
    size_t get_addr_in_mem(size_t posnow);
    varDef() = default;
    // static std::optional<varDef> makeByNode( ComplierParser::DeclarationContext* ast);
};
// struct argDef
// {
//     std::string name;
//     Type type;
//     argDef(std::shared_ptr<peg::Ast> astnode);
// };
struct funcDef : Identifi
{
    // [TODO] funcDefineNode
    // static std::optional<funcDef> makeByNode( ComplierParser::DeclarationContext* ast);
    std::vector<std::string> asms;
    std::vector<varDef> args;
    std::vector<std::vector<varDef>> funcvar_stack;
    size_t max_stack_size = 0; // 预留oldbp
    size_t stack_size_now = 0; // 预留oldbp
    void enter_scope();
    void exit_scope();
    const varDef* lookup_var(const std::string& name) const;
    const varDef* add_var(const varDef& vardef);
    bool add_arg(std::vector<varDef>& vardef);
    funcDef() = default;
};

struct OBJ;
struct linker;
struct exefile;
// 符号表
class SymbolTable
{
    friend OBJ;
    friend linker;
    friend exefile;

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
    ComplierParser::CompilationUnitContext* program;
    // 符号表
    SymbolTable symbol_table;
    // typedef表
    std::unordered_map<std::string, Type> typedefs;
    // 全局偏移
    int bias = 0;
    // 代码生成接口
};