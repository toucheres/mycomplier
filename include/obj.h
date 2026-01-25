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
#include <tree_scoped_map.hpp>
#include <unordered_map>
#include <variant>
#include <vector>
#include <vm.h>
// 前向声明
struct Type;
struct varDef;

struct StructInfo
{
    std::vector<std::pair<std::string, varDef>> members;
    size_t getmemberbias(std::string membername);
    varDef* getmember(std::string name);
};

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
        Undefined,    // 初始
        ID,           // id
        Basic,        // 基本类型
        Pointer,      // 指针类型
        Array,        // 数组类型
        Function,     // 函数类型
        StorageClass, // 存储类, 一般仅用于参数传递而非类型存储
        Struct        // 结构体
    };
    enum class StorageClassSpecifier
    {
        None,
        Typedef,
        Extern,
        Static
    };
    enum class TypeQualifier
    {
        Const,
        Restrict,
        Volatile,
        _Atomic
    };
    Type() = default;
    Type(const Type&) = default;
    Type(Kind kind, BasicType arg);          // for basic
    Type(Kind kind, int arg);                // for arr ,ptr
    Type(Kind kind, std::string arg);        // for id
    Type(Kind kind, std::vector<Type> args); // for function
    Kind kind = Kind::Undefined;
    StorageClassSpecifier storageClassSpecifier = StorageClassSpecifier::None;
    StructInfo structInfo;
    std::string id;
    // 基础类型
    BasicType basic_type; // avilable when kind == Basic
    value_ptr<Type> subType;
    std::vector<Type> args;
    int arr_or_ptr_num = -1;
    size_t alignas_num = 8;
    Type& getTop();
    bool pushTop(const Type& what);
    Type popTop();
    std::string to_string() const;
    size_t getsize() const;
    bool operator==(const Type& other) const;
    Type whthoutID(){
        if (this->kind == Type::Kind::ID)
        {
            return *this->subType;
        }
        return *this;
    }
    Type& removeID()
    {
        if (this->kind == Type::Kind::ID)
        {
            auto tp = *this->subType;
            *this = tp;
        }
        return *this;
    }
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
    enum class Kind
    {
        Local,
        Arg,
        Global
    };
    Kind kind = Kind::Local;
    std::string link_label;
    size_t get_addr_in_stack(size_t posnow);
    varDef() = default;
};
struct funcDef : Identifi
{
    std::vector<std::string> asms;
    Type rettype;
    inline static size_t parpera_for_stack_frame = VCPU<>::size_word;
    size_t max_stack_size = 0;
    size_t stack_size_now = 0;
    funcDef() = default;
    funcDef(const funcDef&) = default;
    funcDef& operator=(const funcDef&) = default;
};

struct DeclRepository
{
    using Label = std::string;

    struct ScopeMeta
    {
        size_t stack_size = 0; // accumulated size for this scope
    };

    DeclRepository() = default;
    DeclRepository(const DeclRepository&) = delete;
    DeclRepository& operator=(const DeclRepository&) = delete;
    DeclRepository(DeclRepository&&) noexcept = default;
    DeclRepository& operator=(DeclRepository&&) noexcept = default;

    tree_scoped_map<std::string, varDef, Label, ScopeMeta> var_decls;
    tree_scoped_map<std::string, varDef, Label, ScopeMeta> extern_decls;
    tree_scoped_map<std::string, varDef, Label, ScopeMeta> static_decls;
    tree_scoped_map<std::string, Type, Label, ScopeMeta> func_decls;
    tree_scoped_map<std::string, Type, Label, ScopeMeta> typedef_decls;
    tree_scoped_map<std::string, Type, Label, ScopeMeta> struct_decls;

    size_t static_label_counter = 0;

    void enter_scope(const Label& label = {});
    void exit_scope();

    bool add_var(varDef v, Type::StorageClassSpecifier storage, funcDef* func_ctx = nullptr,
                 std::optional<std::size_t> arg_index = std::nullopt);
    bool add_func(const Type& t);
    bool add_typedef(const Type& t);
    bool add_struct(const Type& t);

    varDef* find_var(const std::string& name);
    const varDef* find_var(const std::string& name) const;
    Type* find_func(const std::string& name);
    const Type* find_func(const std::string& name) const;
    Type* find_typedef(const std::string& name);
    const Type* find_typedef(const std::string& name) const;
    Type* find_struct(const std::string& name);
    const Type* find_struct(const std::string& name) const;
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
    DeclRepository decls;
    OBJ() = default;
    OBJ(const OBJ&) = delete;
    OBJ& operator=(const OBJ&) = delete;
    OBJ(OBJ&&) noexcept = default;
    OBJ& operator=(OBJ&&) noexcept = default;
    void enter_decl_scope(const std::string& label = {});
    void exit_decl_scope();

    void flush_global_decls();

    std::string global_label(const varDef& v) const;
    std::string global_label(const std::string& name, Type::StorageClassSpecifier storage =
                                                          Type::StorageClassSpecifier::None) const;

    varDef* record_var_decl(const Type& t,
                            Type::StorageClassSpecifier storage = Type::StorageClassSpecifier::None,
                            funcDef* func_ctx = nullptr,
                            std::optional<std::size_t> arg_index = std::nullopt);
    bool record_func_decl(const Type& t);
    bool record_typedef_decl(const std::string& name, const Type& target);
    bool record_struct_decl(const Type& t);

    varDef* lookup_var_decl(const std::string& name);
    const varDef* lookup_var_decl(const std::string& name) const;
    Type* lookup_func_decl(const std::string& name);
    const Type* lookup_func_decl(const std::string& name) const;
    Type* lookup_typedef(const std::string& name);
    const Type* lookup_typedef(const std::string& name) const;
    Type* lookup_struct_decl(const std::string& name);
    const Type* lookup_struct_decl(const std::string& name) const;
    // 代码生成接口
};