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
    Type(Kind kind, BasicType arg);          // for basic
    Type(Kind kind, int arg);                // for arr ,ptr
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
    bool operator==(const Type& other) const;
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
    size_t get_addr_in_stack(size_t posnow);
    varDef() = default;
};
struct funcDef : Identifi
{
    std::vector<std::string> asms;
    std::vector<varDef> args;
    Type rettype;
    std::vector<std::vector<varDef>> funcvar_stack;
    inline static  size_t parpera_for_stack_frame = VCPU<>::size_word;
    size_t max_stack_size = 0;
    size_t stack_size_now = 0;
    void enter_scope();
    void exit_scope();
    const varDef* lookup_var(const std::string& name) const;
    const varDef* add_var(const varDef& vardef);
    bool add_arg(std::vector<varDef>& vardef);
    funcDef(const funcDef&) = default;
    funcDef& operator=(const funcDef&) = default;
    funcDef()
    {
        funcvar_stack.push_back(std::vector<varDef>());
    };
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
    std::unordered_map<std::string, varDef> globalvardef;
    std::unordered_map<std::string, funcDef> globalfuncdef;
    std::unordered_map<std::string, Type> globalvardecl;
    std::unordered_map<std::string, Type> globalfuncdecl;

  public:
    SymbolTable() = default;
    // 添加
    varDef* add_global_var_def(const Type& vardef);
    funcDef* add_global_func_def(const Type& vardef);
    Type* add_global_var_decl(const Type& vardef);
    Type* add_global_func_decl(const Type& vardef);
    // 查找
    Type* lookup_var_decl(const std::string& name);
    Type* lookup_func_decl(const std::string& name);
    funcDef* lookup_func_def(const std::string& name);
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
    // 代码生成接口
};