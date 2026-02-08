#pragma once
#include "CParserBaseVisitor.h"
#include "CParserVisitor.h"
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
// struct Type;
// 新的统一标识符定义（原 varDef 将被替换为 IDdef）
struct IDdef;

struct StructInfo
{
    std::vector<std::pair<std::string, IDdef>> members;
    size_t getmemberbias(std::string membername);
    IDdef* getmember(std::string name);
};

// 存储类说明符从 Type 剥离，独立使用
enum class StorageClassSpecifier
{
    VarDef, // func var均视为var
    StructDef,
    EnumDef,  // enum 类型定义
    EnumConst, // enum 常量
    Typedef,
    Extern,
    Static
};
enum class ValueType
{
    Left,
    Right
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
        Undefined, // 初始
        // ID,           // id (计划移除)
        Basic,    // 基本类型
        Pointer,  // 指针类型
        Array,    // 数组类型
        Function, // 函数类型
        // StorageClass, // 存储类 (计划移除)
        Struct // 结构体
    };
    // 兼容旧代码的别名，后续移除
    using StorageClassSpecifier = ::StorageClassSpecifier;
    enum class TypeQualifier
    {
        Const,
        Restrict,
        Volatile,
        _Atomic
    };
    Type() = default;
    Type(const Type&) = default;
    Type(Kind kind, BasicType arg);           // for basic
    Type(Kind kind, int arg);                 // for arr ,ptr
    Type(Kind kind, std::string arg);         // for id
    Type(Kind kind, std::vector<IDdef> args); // for function
    Kind kind = Kind::Undefined;
    ValueType valueType = ValueType::Right;
    StructInfo structInfo;
    // 标识符名称在迁移到 IDdef 过程中暂时保留，后续可移除
    std::string structID; // 仅用于struct
    // 基础类型
    BasicType basic_type; // avilable when kind == Basic
    value_ptr<Type> subType;
    std::vector<IDdef> args;
    int arr_num = -1; // 仅用于 Array 类型，表示数组长度；Pointer 通过 subType 层数表示
    size_t alignas_num = 8;
    // TODO: remove after callers stop encoding storage on Type
    StorageClassSpecifier storageClassSpecifier = StorageClassSpecifier::VarDef;
    // size_t getAlignas() const;
    Type& getTop();
    bool pushTop(const Type& what);
    Type popTop();
    std::string to_string() const;
    size_t getsize() const;
    bool operator==(const Type& other) const;
    // 兼容旧逻辑的占位：当前类型系统已去除 Kind::ID，直接返回自身
    // Type whthoutID() const
    // {
    //     return *this;
    // }
};
struct IDdef
{
    enum class Kind
    {
        Local,
        Arg,
        Global
    };
    std::string name;
    Type type;
    bool is_defined;
    int addr;
    StorageClassSpecifier storageClassSpecifier = StorageClassSpecifier::VarDef;
    ValueType valueType = ValueType::Right;
    Kind kind = Kind::Local;
    std::string link_label;
    struct FuncInfo
    {
        std::vector<std::string> asms;
        inline static size_t parpera_for_stack_frame = VCPU::size_word;
        size_t max_stack_size = 0;
        size_t stack_size_now = 0;
    } funcInfo;
    size_t get_addr_in_stack(size_t posnow);
    IDdef() = default;
    bool operator==(const IDdef& that) const;
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

    tree_scoped_map<std::string, IDdef, Label, ScopeMeta> var_decls;
    tree_scoped_map<std::string, IDdef, Label, ScopeMeta> extern_decls;
    tree_scoped_map<std::string, IDdef, Label, ScopeMeta> static_decls;
    // tree_scoped_map<std::string, IDdef, Label, ScopeMeta> func_decls;// 与var_decls一起管理
    tree_scoped_map<std::string, IDdef, Label, ScopeMeta> typedef_decls;
    tree_scoped_map<std::string, IDdef, Label, ScopeMeta> struct_decls;
    tree_scoped_map<std::string, IDdef, Label, ScopeMeta> enum_decls;    // enum 类型定义
    tree_scoped_map<std::string, IDdef, Label, ScopeMeta> enum_const_decls; // enum 常量

    size_t static_label_counter = 0;

    void enter_scope(const Label& label = {});
    void exit_scope();
    IDdef* add_ID_decl(const IDdef& IDdef,
                       StorageClassSpecifier storageClassSpecifier = StorageClassSpecifier::VarDef);
    IDdef* find_ID_decl(const std::string& ID, StorageClassSpecifier storageClassSpecifier =
                                                   StorageClassSpecifier::VarDef);
};

struct OBJ;
struct linker;
struct exefile;
// 符号表
struct Symbol_Table
{
    std::unordered_map<std::string, IDdef> globaldef;
    std::unordered_map<std::string, IDdef> globaldecl;
};

struct OBJ
{
    std::string name;
    // AST 根节点
    CParser::CompilationUnitContext* program;
    // 符号表
    Symbol_Table symbol_table;
    DeclRepository decls;
    OBJ() = default;
    OBJ(const OBJ&) = delete;
    OBJ& operator=(const OBJ&) = delete;
    OBJ(OBJ&&) noexcept = default;
    OBJ& operator=(OBJ&&) noexcept = default;
    void enter_decl_scope(const std::string& label = {});
    void exit_decl_scope();
    void flush_global_decls();
};