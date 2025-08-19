#include "complier.hpp"
#include <format>
#include <iostream>

void obj::pushASM(VM::ASM asm_code)
{
    auto ret = VM::getASMmeta(asm_code);
    if (std::to_string(ret.num_args).find('0') == std::string::npos)
    {
        throw;
    }
    content.push_back(ret.name);
    pos++;
}

void obj::pushASM(VM::ASM asm_code, int arg)
{
    auto ret = VM::getASMmeta(asm_code);
    if (std::to_string(ret.num_args).find('1') == std::string::npos)
    {
        throw;
    }
    content.push_back(std::format("{} {}", ret.name, arg));
    pos++;
}

void obj::pushASM(VM::ASM asm_code, int src, int dest)
{
    auto ret = VM::getASMmeta(asm_code);
    if (std::to_string(ret.num_args).find('2') == std::string::npos)
    {
        throw;
    }
    content.push_back(std::format("{} {} {}", ret.name, src, dest));
    pos++;
}

const std::vector<std::string>& obj::get_assembly_vector() const
{
    // 现在content已经是vector了，直接返回引用
    return content;
}
