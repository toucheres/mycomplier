#include "complier.hpp"
#include <algorithm>

// var_defs::eachnamespace 实现
std::expected<var_def*, error> var_defs::eachnamespace::find(const std::string& id)
{
    auto it = std::find_if(var_defines_namespace.begin(), var_defines_namespace.end(),
                          [&id](const var_def& var) { return var.id == id; });
    
    if (it != var_defines_namespace.end())
    {
        return &(*it);
    }
    return std::unexpected(error::doubledefine); // 使用已定义的错误类型
}

std::expected<bool, error> var_defs::eachnamespace::push(var_def var_def_)
{
    // 检查重定义
    auto it = std::find_if(var_defines_namespace.begin(), var_defines_namespace.end(),
                          [&var_def_](const var_def& var) { return var.id == var_def_.id; });
    
    if (it != var_defines_namespace.end())
    {
        return std::unexpected(error::doubledefine);
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
    return std::unexpected(error::doubledefine); // 使用已定义的错误类型
}

std::expected<bool, error> var_defs::push(var_def var_def_)
{
    // 总是添加到当前最内层作用域（可能是全局作用域）
    if (namespace_defines.empty())
    {
        // 这种情况理论上不应该发生，因为构造函数会创建全局作用域
        namespace_defines.emplace_back();
    }
    
    return namespace_defines.back().push(var_def_);
}

std::expected<bool, error> var_defs::push_arg(var_def var_def)
{
    // 参数变量直接添加到当前作用域
    if (namespace_defines.empty())
    {
        into_new_namespace();
    }
    
    return namespace_defines.back().push(var_def);
}

void var_defs::into_new_namespace()
{
    namespace_defines.emplace_back();
}

void var_defs::outto_old_namespace()
{
    if (namespace_defines.size() > 1) // 保持至少一个作用域（全局作用域）
    {
        namespace_defines.pop_back();
    }
}

// obj 实现
void obj::pushASM(VM::ASM ASM)
{
    // 生成汇编指令并推入栈
    switch (ASM)
    {
        case VM::ASM::LI:
            content.push("LI");
            break;
        case VM::ASM::LC:
            content.push("LC");
            break;
        case VM::ASM::SI:
            content.push("SI");
            break;
        case VM::ASM::SC:
            content.push("SC");
            break;
        case VM::ASM::ADD:
            content.push("ADD");
            break;
        case VM::ASM::SUB:
            content.push("SUB");
            break;
        case VM::ASM::MUL:
            content.push("MUL");
            break;
        case VM::ASM::DIV:
            content.push("DIV");
            break;
        case VM::ASM::EQ:
            content.push("EQ");
            break;
        case VM::ASM::NE:
            content.push("NE");
            break;
        case VM::ASM::LT:
            content.push("LT");
            break;
        case VM::ASM::GT:
            content.push("GT");
            break;
        case VM::ASM::LE:
            content.push("LE");
            break;
        case VM::ASM::GE:
            content.push("GE");
            break;
        case VM::ASM::JMP:
            content.push("JMP");
            break;
        case VM::ASM::JZ:
            content.push("JZ");
            break;
        case VM::ASM::JNZ:
            content.push("JNZ");
            break;
        case VM::ASM::CALL:
            content.push("CALL");
            break;
        case VM::ASM::RET:
            content.push("RET");
            break;
        case VM::ASM::PUSH:
            content.push("PUSH");
            break;
        case VM::ASM::IMM:
            content.push("IMM");
            break;
        case VM::ASM::LEA:
            content.push("LEA");
            break;
        default:
            break;
    }
}

void obj::pushASM(VM::ASM ASM, int arg)
{
    pushASM(ASM);
    content.push(std::to_string(arg));
}

void obj::pushASM(VM::ASM ASM, int src, int obj)
{
    pushASM(ASM);
    content.push(std::to_string(src));
    content.push(std::to_string(obj));
}
