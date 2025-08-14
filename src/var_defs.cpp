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
    auto ret = std::find_if(fun_defines.begin(), fun_defines.end(),
                            [&id](fun_def fundef)
                            {
                                if (fundef.id == id)
                                {
                                    return true;
                                }
                                return false;
                            });
    if (ret != fun_defines.end())
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
        // std::cout << "var: \n";
        // std::cout << "addr:" << var_def_.addr << " id:" << var_def_.id
        //           << " type:" << (int)var_def_.type.bt;
        // for (size_t i = 0; i < var_def_.type.ptr_lay; i++)
        // {
        //     std::cout << "*";
        // }
        // std::cout << "\n";
        return true;
    }
    else
    {
        return std::unexpected{ret.error()};
    }
}

std::expected<bool, error> var_defs::push_func_args(var_def var_def)
{
    // 参数变量直接添加到当前作用域，分配地址
    if (namespace_defines.empty())
    {
        into_new_namespace();
    }

    var_def.addr = dy_stack_size;
    auto ret = namespace_defines.back().push(var_def);
    if (ret)
    {
        if (var_def.type.ptr_lay == 0)
        {
            dy_stack_size += Type::size_of_type(var_def.type.bt);
        }
        else
        {
            dy_stack_size += Type::size_of_type(Basic_Type::INT);
        }
        // std::cout << "arg: \n";
        // std::cout << "addr:" << var_def.addr << " id:" << var_def.id
        //           << " type:" << (int)var_def.type.bt;
        // for (size_t i = 0; i < var_def.type.ptr_lay; i++)
        // {
        //     std::cout << "*";
        // }
        // std::cout << "\n";
        return true;
    }
    return ret;
}
void var_defs::clear()
{
    dy_stack_size = 0;
    max_stack_size = 0;
    old_stack_size = 0;
    namespace_defines.clear();
}
void var_defs::into_new_namespace()
{
    namespace_defines.emplace_back();
    old_stack_size = dy_stack_size;
}

size_t var_defs::get_max_size()
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

// obj 实现
// void obj::pushASM(VM::ASM ASM)
// {
//     // 生成汇编指令并推入栈
//     switch (ASM)
//     {
//     case VM::ASM::LI:
//         content.push("LI");
//         break;
//     case VM::ASM::LC:
//         content.push("LC");
//         break;
//     case VM::ASM::SI:
//         content.push("SI");
//         break;
//     case VM::ASM::SC:
//         content.push("SC");
//         break;
//     case VM::ASM::ADD:
//         content.push("ADD");
//         break;
//     case VM::ASM::SUB:
//         content.push("SUB");
//         break;
//     case VM::ASM::MUL:
//         content.push("MUL");
//         break;
//     case VM::ASM::DIV:
//         content.push("DIV");
//         break;
//     case VM::ASM::EQ:
//         content.push("EQ");
//         break;
//     case VM::ASM::NE:
//         content.push("NE");
//         break;
//     case VM::ASM::LT:
//         content.push("LT");
//         break;
//     case VM::ASM::GT:
//         content.push("GT");
//         break;
//     case VM::ASM::LE:
//         content.push("LE");
//         break;
//     case VM::ASM::GE:
//         content.push("GE");
//         break;
//     case VM::ASM::JMP:
//         content.push("JMP");
//         break;
//     case VM::ASM::JZ:
//         content.push("JZ");
//         break;
//     case VM::ASM::JNZ:
//         content.push("JNZ");
//         break;
//     case VM::ASM::CALL:
//         content.push("CALL");
//         break;
//     case VM::ASM::RET:
//         content.push("RET");
//         break;
//     case VM::ASM::PUSH:
//         content.push("PUSH");
//         break;
//     case VM::ASM::IMM:
//         content.push("IMM");
//         break;
//     case VM::ASM::LEA:
//         content.push("LEA");
//         break;
//     default:
//         break;
//     }
// }

// void obj::pushASM(VM::ASM ASM, int arg)
// {
//     pushASM(ASM);
//     content.push(std::to_string(arg));
// }

// void obj::pushASM(VM::ASM ASM, int src, int obj)
// {
//     pushASM(ASM);
//     content.push(std::to_string(src));
//     content.push(std::to_string(obj));
// }
