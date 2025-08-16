#include "complier.hpp"
#include <algorithm>
#include <iostream>
// var_defs::eachnamespace 实现
std::expected<var_def*, error> var_defs::eachnamespace::find(const std::string& id)
{
    auto it = std::find_if(var_defines_namespace.begin(), var_defines_namespace.end(),
                           [&id](const var_def& var) { return var.id == id; });

    if (it != var_defines_namespace.end())
    {
        return &(*it);
    }
    return std::unexpected(error::doubledefined); // 使用已定义的错误类型
}

std::expected<fun_def, error> fun_defs::find(const std::string& id)
{
    std::optional<fun_def> ret;
    for (const auto& each : fun_defines)
    {
        if (each.id == id)
        {
            ret = each;
            break;
        }
    }
    if (ret)
    {
        return *ret;
    }
    return std::unexpected(error::undefinedfun);
}
std::expected<bool, error> fun_defs::push(fun_def fun_def)
{
    if (find(fun_def.id))
    {
        return std::unexpected(error::doubledefined);
    }
    fun_defines.push_back(fun_def);

    // std::cout << "fun_def:\n";
    // std::cout << "addr:" << fun_def.addr << " id:" << fun_def.id
    //           << " type:" << (int)fun_def.type.bt;
    // for (int i = 0; i < fun_def.type.ptr_lay; i++)
    // {
    //     std::cout << "*";
    // }
    // std::cout << '\n';
    return true;
}

std::expected<bool, error> var_defs::eachnamespace::push(var_def var_def_)
{
    // 检查重定义
    auto it = std::find_if(var_defines_namespace.begin(), var_defines_namespace.end(),
                           [&var_def_](const var_def& var) { return var.id == var_def_.id; });

    if (it != var_defines_namespace.end())
    {
        return std::unexpected(error::doubledefined);
    }

    var_defines_namespace.push_back(var_def_);
    return true;
}

// var_defs 实现
std::expected<var_def*, error> var_defs::find(const std::string& id)
{
    max_stack_size = std::max(max_stack_size, dy_stack_size);
    // 从当前作用域向上查找（从最内层到最外层，包括全局作用域）
    for (auto it = namespace_defines.rbegin(); it != namespace_defines.rend(); ++it)
    {
        auto result = it->find(id);
        if (result)
        {
            return result;
        }
    }

    // 如果所有作用域都没找到，返回错误
    return std::unexpected(error::doubledefined); // 使用已定义的错误类型
}

std::expected<bool, error> var_defs::push(var_def var_def_)
{
    // 总是添加到当前最内层作用域（可能是全局作用域）
    if (namespace_defines.empty())
    {
        // 这种情况理论上不应该发生，因为构造函数会创建全局作用域
        namespace_defines.emplace_back();
    }
    var_def_.addr = dy_stack_size;
    auto ret = namespace_defines.back().push(var_def_);
    if (ret)
    {
        if (var_def_.type.ptr_lay == 0)
        {
            dy_stack_size += Type::size_of_type(var_def_.type.bt);
        }
        else
        {
            dy_stack_size += Type::size_of_type(Basic_Type::INT);
        }
        max_stack_size = std::max(max_stack_size, dy_stack_size);
        return true;
    }
    else
    {
        return std::unexpected{ret.error()};
    }
}

// std::expected<bool, error> var_defs::push_func_args(var_def var_def)
// {
//     // 参数变量直接添加到当前作用域，分配地址
//     if (namespace_defines.empty())
//     {
//         into_new_namespace();
//     }

//     var_def.addr = dy_stack_size;
//     auto ret = namespace_defines.back().push(var_def);
//     if (ret)
//     {
//         if (var_def.type.ptr_lay == 0)
//         {
//             dy_stack_size += Type::size_of_type(var_def.type.bt);
//         }
//         else
//         {
//             dy_stack_size += Type::size_of_type(Basic_Type::INT);
//         }
//         return true;
//     }
//     return ret;
// }
void var_defs::clear()
{
    dy_stack_size = 0;
    max_stack_size = 0;
    old_stack_size = 0;
    namespace_defines.clear();
}
std::expected<bool, error> var_defs::push_func_args(std::vector<var_def> var_defs)
{
    for (int i = 0; i < var_defs.size(); i++)
    {
        var_defs[i].addr = i - var_defs.size();
        if (!namespace_defines[0].push(var_defs[i]))
        {
            return std::unexpected(error::undefinedvar);
        }
    }
    return true;
}
void var_defs::into_new_namespace()
{
    max_stack_size = std::max(max_stack_size, dy_stack_size);
    namespace_defines.emplace_back();
    old_stack_size = dy_stack_size;
}

int var_defs::get_max_size()
{
    return max_stack_size;
}

void var_defs::outto_old_namespace()
{
    if (namespace_defines.size() > 1) // 保持至少一个作用域（全局作用域）
    {
        namespace_defines.pop_back();
    }
    max_stack_size = std::max(max_stack_size, dy_stack_size);
    dy_stack_size = old_stack_size;
}