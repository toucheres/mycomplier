#include "obj.h"
#include "complier.hpp"
#include <format>
#include <iostream>

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
    newFunc.add_arg(args);
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