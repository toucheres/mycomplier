#include "obj.h"
#include "complier.hpp"
#include <algorithm>
#include <format>

namespace
{
Type strip_id_preserve(const Type& t)
{
    Type cleaned = t;
    cleaned.removeID();
    cleaned.id = t.id;
    return cleaned;
}

template <class MapT> void enter_scope_all(MapT& map, const std::string& label)
{
    if (label.empty())
    {
        map.enter_scope();
    }
    else
    {
        map.enter_scope(label);
    }
}

inline varDef::Kind kind_from_depth(std::size_t depth_zero_based)
{
    if (depth_zero_based == 0)
    {
        return varDef::Kind::Global;
    }
    if (depth_zero_based == 1)
    {
        return varDef::Kind::Arg;
    }
    return varDef::Kind::Local;
}
} // namespace

void DeclRepository::enter_scope(const Label& label)
{
    enter_scope_all(var_decls, label);
    enter_scope_all(func_decls, label);
    enter_scope_all(extern_decls, label);
    enter_scope_all(static_decls, label);
    enter_scope_all(typedef_decls, label);
}

void DeclRepository::exit_scope()
{
    var_decls.out_scope();
    func_decls.out_scope();
    extern_decls.out_scope();
    static_decls.out_scope();
    typedef_decls.out_scope();
}

static tree_scoped_map<std::string, varDef, DeclRepository::Label, DeclRepository::ScopeMeta>*
pick_storage_map(DeclRepository& repo, Type::StorageClassSpecifier storage)
{
    switch (storage)
    {
    case Type::StorageClassSpecifier::Extern:
        return &repo.extern_decls;
    case Type::StorageClassSpecifier::Static:
        return &repo.static_decls;
    default:
        return &repo.var_decls;
    }
}

bool DeclRepository::add_var(varDef v, Type::StorageClassSpecifier storage)
{
    v.type = strip_id_preserve(v.type);
    auto* map = pick_storage_map(*this, storage);
    auto depth = map->scope_depth() - 1; // zero-based: 0 root, 1 params, else locals
    v.type.storageClassSpecifier = storage;
    v.kind = kind_from_depth(depth);
    if (storage != Type::StorageClassSpecifier::Extern)
    {
        map->current_scope_info().stack_size += v.type.getsize();
    }
    return map->add(v.name, v);
}

bool DeclRepository::add_func(const Type& t)
{
    auto cleaned = strip_id_preserve(t);
    cleaned.id = t.id;
    return func_decls.add(t.id, cleaned);
}

bool DeclRepository::add_typedef(const Type& t)
{
    auto cleaned = strip_id_preserve(t);
    cleaned.id = t.id;
    return typedef_decls.add(t.id, cleaned);
}

varDef* DeclRepository::find_var(const std::string& name)
{
    if (auto* p = var_decls.find(name))
    {
        return p;
    }
    if (auto* p = static_decls.find(name))
    {
        return p;
    }
    return extern_decls.find(name);
}

const varDef* DeclRepository::find_var(const std::string& name) const
{
    if (auto* p = var_decls.find(name))
    {
        return p;
    }
    if (auto* p = static_decls.find(name))
    {
        return p;
    }
    return extern_decls.find(name);
}

Type* DeclRepository::find_func(const std::string& name) { return func_decls.find(name); }

const Type* DeclRepository::find_func(const std::string& name) const
{
    return func_decls.find(name);
}

Type* DeclRepository::find_typedef(const std::string& name)
{
    return typedef_decls.find(name);
}

const Type* DeclRepository::find_typedef(const std::string& name) const
{
    return typedef_decls.find(name);
}

varDef* SymbolTable::add_global_var_def(const Type& vardef)
{
    if (vardef.kind != Type::Kind::ID)
    {
        return nullptr; // 必须提供有效的标识符
    }

    // 检查是否已存在变量定义
    if (globalvardef.find(vardef.id) != globalvardef.end())
    {
        return nullptr; // 变量已定义
    }

    // 创建变量定义
    varDef newVar;
    newVar.name = vardef.id;
    newVar.type = *vardef.subType;
    newVar.is_defined = true;
    newVar.kind = varDef::Kind::Global;

    // 添加到全局变量定义与声明表
    globalvardef[vardef.id] = newVar;
    globalvardecl[vardef.id] = *vardef.subType;

    return &globalvardef[vardef.id];
}

funcDef* SymbolTable::add_global_func_def(const Type& vardef)
{
    if (vardef.kind != Type::Kind::ID || vardef.subType == nullptr ||
        vardef.subType->kind != Type::Kind::Function)
    {
        return nullptr; // 必须是函数类型
    }

    // 检查是否已存在函数定义
    auto it = globalfuncdef.find(vardef.id);
    if (it != globalfuncdef.end() && it->second.is_defined)
    {
        return nullptr; // 函数已定义
    }

    // 创建或更新函数定义
    funcDef newFunc;
    newFunc.name = vardef.id;
    newFunc.type = *vardef.subType;
    newFunc.is_defined = true;
    newFunc.rettype = *vardef.subType->subType;
    std::vector<varDef> args;
    for(auto&each:vardef.subType->args)
    {
        varDef tp;
        tp.name = each.id;
        tp.type = *each.subType;
        tp.is_defined = true;
        args.push_back(tp);
    }
    newFunc.args = args;
    // [TODO] 参数大小不一定恒定2字
    for (std::size_t i = 0; i < newFunc.args.size(); ++i)
    {
        newFunc.args[i].addr = VCPU<>::size_word * (static_cast<int>(i) + 2);
        newFunc.args[i].kind = varDef::Kind::Arg;
    }
    // 添加到全局函数定义/声明表
    globalfuncdef[vardef.id] = newFunc;
    globalfuncdecl[vardef.id] = *vardef.subType;
    return &globalfuncdef[vardef.id];
}

Type* SymbolTable::add_global_var_decl(const Type& vardef)
{
    if (vardef.kind != Type::Kind::ID)
    {
        return nullptr; // 必须提供有效的标识符
    }
    // 如果已存在变量声明
    if (globalvardecl.find(vardef.id) != globalvardecl.end())
    {
        if (*globalvardecl[vardef.id].subType == *vardef.subType)
        {
            return globalvardecl[vardef.id].subType.get();
        }
        else
        {
            return nullptr;
        }
    }
    // 添加新变量声明
    globalvardecl[vardef.id] = vardef;
    return &globalvardecl[vardef.id];
}

Type* SymbolTable::add_global_func_decl(const Type& vardef)
{
    if (vardef.kind != Type::Kind::ID || vardef.subType == nullptr ||
        vardef.subType->kind != Type::Kind::Function)
    {
        return nullptr; // 必须是函数类型
    }

    // 如果已存在函数声明，检查兼容性并更新
    if (globalfuncdecl.find(vardef.id) != globalfuncdecl.end())
    {
        if (*vardef.subType == *globalfuncdecl[vardef.id].subType)
        {
            globalfuncdecl[vardef.id] = vardef;
        }
        return &globalfuncdecl[vardef.id];
    }

    // 添加新函数声明
    globalfuncdecl[vardef.id] = vardef;
    return &globalfuncdecl[vardef.id];
}

Type* SymbolTable::lookup_var_decl(const std::string& name)
{
    // 首先查找变量定义
    auto defIt = globalvardecl.find(name);
    if (defIt != globalvardecl.end())
    {
        return &(defIt->second);
    }
    return nullptr; // 未找到
}

Type* SymbolTable::lookup_func_decl(const std::string& name)
{
    // 首先查找函数定义
    auto defIt = globalfuncdecl.find(name);
    if (defIt != globalfuncdecl.end())
    {
        return &defIt->second;
    }
    return nullptr; // 未找到
}
funcDef* SymbolTable::lookup_func_def(const std::string& name)
{
    // 首先查找函数定义
    auto defIt = globalfuncdef.find(name);
    if (defIt != globalfuncdef.end())
    {
        return &defIt->second;
    }
    return nullptr; // 未找到
}

void OBJ::enter_decl_scope(const std::string& label)
{
    decls.enter_scope(label);
}

void OBJ::exit_decl_scope()
{
    decls.exit_scope();
}

varDef* OBJ::record_var_decl(const Type& t, Type::StorageClassSpecifier storage,
                             funcDef* func_ctx, std::optional<std::size_t> arg_index)
{
    varDef v;
    v.name = t.id;
    v.type = t;
    v.is_defined = true;

    if (!decls.add_var(v, storage))
    {
        return nullptr;
    }

    auto* slot = decls.find_var(v.name);
    if (!slot)
    {
        return nullptr;
    }

    // Assign addresses for locals/args when inside a function.
    if (func_ctx)
    {
        if (slot->kind == varDef::Kind::Arg && arg_index)
        {
            slot->addr = static_cast<int>(VCPU<>::size_word * (static_cast<int>(*arg_index) + 2));
        }
        else if (slot->kind == varDef::Kind::Local &&
                 storage != Type::StorageClassSpecifier::Extern)
        {
            const auto new_addr =
                -static_cast<int>(slot->get_addr_in_stack(func_ctx->stack_size_now));
            slot->addr = new_addr;
            func_ctx->stack_size_now = static_cast<size_t>(-new_addr);
            func_ctx->max_stack_size = std::max(func_ctx->max_stack_size, func_ctx->stack_size_now);
        }

        // Keep func_ctx->args in sync when applicable.
        for (auto& arg : func_ctx->args)
        {
            if (arg.name == slot->name)
            {
                arg.addr = slot->addr;
                arg.kind = slot->kind;
            }
        }
    }

    return slot;
}

bool OBJ::record_func_decl(const Type& t)
{
    return decls.add_func(t);
}

bool OBJ::record_typedef_decl(const std::string& name, const Type& target)
{
    Type tp = target;
    tp.id = name;
    return decls.add_typedef(tp);
}

varDef* OBJ::lookup_var_decl(const std::string& name) { return decls.find_var(name); }

const varDef* OBJ::lookup_var_decl(const std::string& name) const
{
    return decls.find_var(name);
}

Type* OBJ::lookup_func_decl(const std::string& name) { return decls.find_func(name); }

const Type* OBJ::lookup_func_decl(const std::string& name) const
{
    return decls.find_func(name);
}

Type* OBJ::lookup_typedef(const std::string& name) { return decls.find_typedef(name); }

const Type* OBJ::lookup_typedef(const std::string& name) const
{
    return decls.find_typedef(name);
}

void OBJ::flush_global_decls()
{
    // variables: normal
    decls.var_decls.for_each_scope([this](std::size_t depth, auto& entries, auto&, const auto&)
                                   {
                                       if (depth != 0)
                                       {
                                           return;
                                       }
                                       for (auto& [name, v] : entries)
                                       {
                                           Type t = v.type;
                                           t.id = name;
                                           symbol_table.add_global_var_def(t);
                                       }
                                   });

    // static variables treated as global definitions for now.
    decls.static_decls.for_each_scope(
        [this](std::size_t depth, auto& entries, auto&, const auto&)
        {
            if (depth != 0)
            {
                return;
            }
            for (auto& [name, v] : entries)
            {
                Type t = v.type;
                t.id = name;
                symbol_table.add_global_var_def(t);
            }
        });

    // extern declarations become decl entries
    decls.extern_decls.for_each_scope([this](std::size_t depth, auto& entries, auto&, const auto&)
                                      {
                                          if (depth != 0)
                                          {
                                              return;
                                          }
                                          for (auto& [name, v] : entries)
                                          {
                                              Type t = v.type;
                                              t.id = name;
                                              symbol_table.add_global_var_decl(t);
                                          }
                                      });

    // function prototypes/defs recorded as declarations
    decls.func_decls.for_each_scope([this](std::size_t depth, auto& entries, auto&, const auto&)
                                    {
                                        if (depth != 0)
                                        {
                                            return;
                                        }
                                        for (auto& [name, t] : entries)
                                        {
                                            (void)symbol_table.add_global_func_decl(t);
                                        }
                                    });
}